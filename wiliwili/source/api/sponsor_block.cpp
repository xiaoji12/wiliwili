//
// 空降助手（SponsorBlock）实现
//

#include "api/sponsor_block.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include <borealis.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "api/bilibili/util/http.hpp"
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

}  // namespace wiliwili
