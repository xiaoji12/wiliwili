//
// 空降助手（SponsorBlock）实现
//

#include "api/sponsor_block.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>

#include <borealis.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "api/bilibili/util/http.hpp"
#include "utils/config_helper.hpp"
#include "utils/sha256_helper.hpp"
#include "view/mpv_core.hpp"

namespace wiliwili {

/// 请求超时（毫秒）
static constexpr int SPONSOR_TIMEOUT_MS = 6000;

/// 跳转冷却（秒），避免与用户手动拖动进度条互相打架
static constexpr double SEEK_COOLDOWN = 1.5;

/// 分段起始判定容差（秒），进度事件为整秒触发，需要一点提前量
static constexpr double START_TOLERANCE = 0.3;

/// 距离分段结尾多久内不再跳转（秒），避免跳到片段末尾又被判定命中
static constexpr double END_MARGIN = 0.5;

SponsorBlock& SponsorBlock::instance() {
    static SponsorBlock inst;
    return inst;
}

void SponsorBlock::reset() {
    std::lock_guard<std::mutex> lock(mtx);
    currentBvid.clear();
    segments.clear();
    skipped.clear();
    lastSeekAt = -100;
    ++version;
}

void SponsorBlock::load(const std::string& bvid) {
    if (bvid.empty() || !ENABLED) {
        reset();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        // 同一个视频不重复请求：切换清晰度、切分P 时都会再次触发 load
        if (currentBvid == bvid) return;

        currentBvid = bvid;
        segments.clear();
        skipped.clear();
        lastSeekAt = -100;
        ++version;
    }

    // 只发送 SHA256(bvid) 的前 4 位十六进制
    const std::string hashPrefix = sha256Hex(bvid).substr(0, 4);
    requestSegments(hashPrefix, bvid);
}

void SponsorBlock::requestSegments(const std::string& hashPrefix, const std::string& bvid) {
    // 使用独立的 cpr session，而不是 HTTP::createSession()：
    //   - 后者固定携带 B 站的 Referer / Origin 请求头，发往第三方域名不合适
    //   - 这里只继承用户在设置中配置的代理与 SSL 校验开关，保证网络环境一致
    auto session = std::make_shared<cpr::Session>();
    session->SetUrl(cpr::Url{SERVER + "/api/skipSegments/" + hashPrefix});
    session->SetHeader(cpr::Header{{"User-Agent", "wiliwili"}});
    session->SetTimeout(cpr::Timeout{SPONSOR_TIMEOUT_MS});
    session->SetConnectTimeout(cpr::ConnectTimeout{SPONSOR_TIMEOUT_MS});
    // cpr::Proxies 没有 empty()，空值表示不使用代理，直接设置即可
    session->SetProxies(bilibili::HTTP::PROXIES);
    session->SetVerifySsl(bilibili::HTTP::VERIFY);

    // cpr 内部会持有 session 的 shared_ptr，回调期间 session 不会析构
    session->GetCallback([this, bvid](const cpr::Response& r) {
        if (r.error) {
            brls::Logger::warning("SponsorBlock: request failed: {}", r.error.message);
            return;
        }
        if (r.status_code == 200) {
            this->applySegments(r.text, bvid);
        } else if (r.status_code == 404) {
            // 该视频还没有人提交分段，属正常情况
            brls::Logger::info("SponsorBlock: no segment for {}", bvid);
        } else {
            brls::Logger::warning("SponsorBlock: http status {}", r.status_code);
        }
    });
}

void SponsorBlock::applySegments(const std::string& body, const std::string& bvid) {
    std::vector<SponsorSegment> found;

    try {
        // 注意：这里返回的是裸 JSON 数组，不是 B 站接口的 {code, data} 包装
        auto root = nlohmann::json::parse(body);
        if (!root.is_array()) {
            brls::Logger::warning("SponsorBlock: unexpected response body");
            return;
        }

        for (const auto& item : root) {
            if (!item.is_object()) continue;

            // 一个哈希桶里包含多个视频，只取当前这个
            if (item.value("videoID", std::string{}) != bvid) continue;
            if (!item.contains("segments") || !item["segments"].is_array()) continue;

            for (const auto& s : item["segments"]) {
                if (!s.is_object()) continue;
                if (!s.contains("segment") || !s["segment"].is_array()) continue;
                // 单元素数组表示整片，跳过不处理
                if (s["segment"].size() < 2) continue;

                const std::string actionType = s.value("actionType", std::string{"skip"});
                if (actionType != "skip" && actionType != "poi") continue;

                const std::string category = s.value("category", std::string{});
                if (std::find(CATEGORIES.begin(), CATEGORIES.end(), category) == CATEGORIES.end()) continue;

                SponsorSegment seg;
                seg.start      = s["segment"][0].get<double>();
                seg.end        = s["segment"][1].get<double>();
                seg.uuid       = s.value("UUID", std::string{});
                seg.category   = category;
                seg.actionType = actionType;

                if (seg.end <= seg.start) continue;
                found.emplace_back(seg);
            }
        }
    } catch (const std::exception& e) {
        brls::Logger::error("SponsorBlock: parse error: {}", e.what());
        return;
    }

    std::sort(found.begin(), found.end(),
              [](const SponsorSegment& a, const SponsorSegment& b) { return a.start < b.start; });

    std::lock_guard<std::mutex> lock(mtx);
    // 请求期间可能已经切换到别的视频，丢弃过期结果
    if (currentBvid != bvid) return;

    segments = std::move(found);
    skipped.assign(segments.size(), false);
    ++version;
    brls::Logger::info("SponsorBlock: {} segment(s) for {}", segments.size(), bvid);
}

void SponsorBlock::onTimeUpdate(double timeSec) {
    if (!ENABLED) return;

    // 用户正在拖动进度条，不要抢跳
    if (MPVCore::instance().video_seeking) return;

    SponsorSegment target;
    bool hit = false;

    {
        std::lock_guard<std::mutex> lock(mtx);
        if (segments.empty()) return;
        if (timeSec - lastSeekAt < SEEK_COOLDOWN) return;

        for (size_t i = 0; i < segments.size(); ++i) {
            if (skipped[i]) continue;

            const SponsorSegment& seg = segments[i];
            if (timeSec >= seg.start - START_TOLERANCE && timeSec < seg.end - END_MARGIN) {
                skipped[i] = true;
                lastSeekAt = timeSec;
                target     = seg;
                hit        = true;
                break;
            }
        }
    }

    if (!hit) return;

    brls::Logger::info("SponsorBlock: skip [{}] {:.1f}s -> {:.1f}s", target.category, target.start, target.end);

    // 跳到分段结尾（向上取整，避免停在片段内部）
    MPVCore::instance().seek(static_cast<int64_t>(std::ceil(target.end)));
    MPVCore::instance().showOsdText(categoryName(target.category), 2000);
}

std::vector<SponsorSegment> SponsorBlock::getSegments() {
    std::lock_guard<std::mutex> lock(mtx);
    return segments;
}

uint64_t SponsorBlock::getVersion() {
    std::lock_guard<std::mutex> lock(mtx);
    return version;
}

uint32_t SponsorBlock::colorForCategory(const std::string& category) {
    // 与 SponsorBlock 官方浏览器扩展的 barTypes 配色完全一致，
    // 保证用户在网页端与客户端看到的颜色是同一套。
    // 来源：SponsorBlock src/config.ts -> barTypes[*].color
    if (category == "sponsor") return 0x00D400;           // 绿     —— 赞助 / 恰饭
    if (category == "selfpromo") return 0xFFFF00;         // 黄     —— 自我推广
    if (category == "interaction") return 0xCC00FF;       // 紫     —— 互动提醒
    if (category == "intro") return 0x00FFFF;             // 青     —— 开场
    if (category == "outro") return 0x0202ED;             // 蓝     —— 结尾
    if (category == "preview") return 0x008FD6;           // 浅蓝   —— 预告
    if (category == "filler") return 0x7300FF;            // 紫罗兰 —— 离题内容
    if (category == "music_offtopic") return 0xFF9900;    // 橙     —— 非音乐部分
    if (category == "poi_highlight") return 0xFF1684;     // 粉     —— 高能时刻
    if (category == "exclusive_access") return 0x008A5C;  // 深绿   —— 独占内容
    return 0xAAAAAA;                                      // 灰     —— 未知分类
}

std::string SponsorBlock::categoryName(const std::string& category) {
    if (category == "sponsor") return "已跳过: 赞助内容";
    if (category == "selfpromo") return "已跳过: 自我推广";
    if (category == "interaction") return "已跳过: 互动提醒";
    if (category == "intro") return "已跳过: 开场";
    if (category == "outro") return "已跳过: 结尾";
    if (category == "preview") return "已跳过: 预告";
    if (category == "filler") return "已跳过: 离题内容";
    if (category == "music_offtopic") return "已跳过: 非音乐部分";
    if (category == "poi_highlight") return "已跳过: 高能时刻";
    if (category == "exclusive_access") return "已跳过: 独占内容";
    return "已跳过: " + category;
}

const std::vector<std::string>& SponsorBlock::submittableCategories() {
    // 与上游 BilibiliSponsorBlock 的提交分类保持一致，顺序即提交对话框里的顺序。
    // 这里不放 exclusive_access：它需要「独占内容」的额外授权说明，误提交率高。
    static const std::vector<std::string> list = {
        "sponsor",  "selfpromo",  "interaction",   "intro",         "outro",
        "preview",  "filler",     "music_offtopic", "poi_highlight",
    };
    return list;
}

std::string SponsorBlock::randomId(size_t length) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static std::mt19937_64 gen{std::random_device{}()};
    static std::mutex genMtx;

    std::lock_guard<std::mutex> lock(genMtx);
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) result.push_back(charset[dist(gen)]);
    return result;
}

std::string SponsorBlock::userID() {
    auto& conf = ProgramConfig::instance();

    // 长度不对（首次运行、配置被清空或被手工改坏）就重新生成一个
    std::string id = conf.getSettingItem<std::string>(SettingItem::PLAYER_SPONSOR_USER_ID, std::string{});
    if (id.size() == 36) return id;

    id = randomId(36);
    conf.setSettingItem(SettingItem::PLAYER_SPONSOR_USER_ID, id);
    brls::Logger::info("SponsorBlock: generated a new anonymous user ID");
    return id;
}

void SponsorBlock::addLocalSegment(const std::string& bvid, const SponsorSegment& seg) {
    std::lock_guard<std::mutex> lock(mtx);
    // 提交期间可能已经切到别的视频，丢弃
    if (currentBvid != bvid) return;

    segments.emplace_back(seg);
    std::sort(segments.begin(), segments.end(),
              [](const SponsorSegment& a, const SponsorSegment& b) { return a.start < b.start; });
    skipped.assign(segments.size(), false);
    ++version;
}

void SponsorBlock::submit(const std::string& bvid, int64_t cid, double start, double end, const std::string& category,
                          double videoDuration, std::function<void(bool, SponsorSubmitError)> callback) {
    auto done = [callback](bool ok, SponsorSubmitError err) {
        if (callback) callback(ok, err);
    };

    if (bvid.empty() || end <= start || start < 0) {
        brls::Logger::warning("SponsorBlock: invalid submission {:.1f} -> {:.1f}", start, end);
        done(false, SponsorSubmitError::Invalid);
        return;
    }

    // 服务端要求 segment 落在 [0, duration] 内
    if (videoDuration > 0 && end > videoDuration) end = videoDuration;
    if (end <= start) {
        done(false, SponsorSubmitError::Invalid);
        return;
    }

    const std::string actionType = category == "poi_highlight" ? "poi" : "skip";
    const std::string localUuid  = randomId(36);

    // 注意：协议里 cid 是字符串而不是数字
    // （上游 BilibiliSponsorBlock 的 `type CID = string`，服务端返回的也是 "cid":"30808542485"）
    const std::string cidStr = std::to_string(cid);

    nlohmann::json segment;
    segment["cid"]        = cidStr;
    segment["segment"]    = nlohmann::json::array({start, end});
    segment["UUID"]       = localUuid;
    segment["category"]   = category;
    segment["actionType"] = actionType;

    nlohmann::json body;
    body["videoID"]       = bvid;
    body["cid"]           = cidStr;
    body["userID"]        = userID();
    body["segments"]      = nlohmann::json::array({segment});
    body["videoDuration"] = videoDuration;
    body["userAgent"]     = "wiliwili/" + APPVersion::instance().getVersionStr();

    // 与 GET 一样使用独立 session：不要带上 B 站的 Referer / Origin
    auto session = std::make_shared<cpr::Session>();
    session->SetUrl(cpr::Url{SERVER + "/api/skipSegments"});
    session->SetHeader(cpr::Header{{"User-Agent", "wiliwili"}, {"Content-Type", "application/json"}});
    session->SetBody(cpr::Body{body.dump()});
    session->SetTimeout(cpr::Timeout{SPONSOR_TIMEOUT_MS});
    session->SetConnectTimeout(cpr::ConnectTimeout{SPONSOR_TIMEOUT_MS});
    session->SetProxies(bilibili::HTTP::PROXIES);
    session->SetVerifySsl(bilibili::HTTP::VERIFY);

    session->PostCallback([this, bvid, category, actionType, localUuid, start, end,
                           done](const cpr::Response& r) {
        if (r.error) {
            brls::Logger::warning("SponsorBlock: submit failed: {}", r.error.message);
            done(false, SponsorSubmitError::Network);
            return;
        }
        if (r.status_code != 200) {
            brls::Logger::warning("SponsorBlock: submit rejected, http {}", r.status_code);
            done(false, SponsorSubmitError::Rejected);
            return;
        }

        // 成功时服务端返回与提交数量等长的数组，带上它分配的 UUID
        SponsorSegment seg;
        seg.start      = start;
        seg.end        = end;
        seg.category   = category;
        seg.actionType = actionType;
        seg.uuid       = localUuid;

        try {
            auto root = nlohmann::json::parse(r.text);
            if (root.is_array() && !root.empty() && root[0].is_object()) {
                seg.uuid = root[0].value("UUID", localUuid);
            }
        } catch (const std::exception& e) {
            // 返回值解析失败不影响提交本身，本地 UUID 继续用
            brls::Logger::warning("SponsorBlock: submit response parse error: {}", e.what());
        }

        this->addLocalSegment(bvid, seg);
        brls::Logger::info("SponsorBlock: submitted [{}] {:.1f}s -> {:.1f}s for {}", category, start, end, bvid);
        done(true, SponsorSubmitError::None);
    });
}

}  // namespace wiliwili
