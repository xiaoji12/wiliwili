//
// 空降助手（SponsorBlock）
//
// 接入 BilibiliSponsorBlock 的服务端协议，自动跳过 B 站视频中的
// 赞助、恰饭、开场、结尾、预告等片段。
//
// 协议要点：
//   GET {SERVER}/api/skipSegments/{SHA256(bvid) 前 4 位十六进制}
//   服务端返回该哈希桶下的全部视频分段（裸 JSON 数组），客户端按 bvid 本地过滤。
//   只发送 4 位哈希前缀是为了避免向第三方泄露用户观看的完整视频 ID。
//   无数据时返回 HTTP 404。
//
//   POST {SERVER}/api/skipSegments
//   提交一个分段，body 为
//     { videoID, cid, userID, segments: [{cid, segment:[s,e], UUID, category, actionType}],
//       videoDuration, userAgent }
//   成功时返回 200 + 与提交数量等长的分段数组（含服务端分配的 UUID）。
//   提交使用本地随机生成的匿名 userID，不需要登录 B 站账号。
//

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace wiliwili {

/// 单个待跳过的分段
struct SponsorSegment {
    double start      = 0;  // 起始时间（秒）
    double end        = 0;  // 结束时间（秒）
    std::string uuid;       // 服务端分段 UUID
    std::string category;   // 分类，如 sponsor / intro / outro
    std::string actionType; // 动作类型，目前只处理 skip / poi
};

/// 提交失败的原因，交由 UI 层翻译成提示文案
enum class SponsorSubmitError {
    None,      // 成功
    Invalid,   // 本地参数不合法（起止时间倒置、视频 ID 为空等）
    Network,   // 网络错误 / 超时
    Rejected,  // 服务端拒绝（非 200）
};

class SponsorBlock {
public:
    static SponsorBlock& instance();

    /**
     * 开始为某个视频加载分段。
     * 在播放地址就绪时调用；同一 bvid 重复调用不会重复请求（切换清晰度会重复触发）。
     * 请求是异步的，失败静默降级，不影响播放。
     */
    void load(const std::string& bvid);

    /// 清空当前视频的分段状态
    void reset();

    /**
     * 播放进度回调，由播放器每秒调用一次。
     * 命中分段时执行跳转，并弹出提示。
     * @param timeSec 当前播放时间（秒）
     */
    void onTimeUpdate(double timeSec);

    /// 当前视频的分段列表（副本，用于在进度条上绘制标记）
    std::vector<SponsorSegment> getSegments();

    /**
     * 分段集合的版本号，每次内容变化时自增。
     * UI 侧缓存上一次的版本号，只有变化时才重新拉取分段并重建进度条标记，
     * 避免在每帧的绘制路径上反复加锁。
     */
    uint64_t getVersion();

    /**
     * 提交一个分段到服务端。
     * 异步执行，callback 在 cpr 的后台线程里被调用，UI 侧需要自行切回主线程。
     * 提交成功后会把该分段并入本地列表，进度条上立即生效。
     *
     * @param bvid          视频 ID
     * @param cid           分 P ID
     * @param start         起始时间（秒）
     * @param end           结束时间（秒）
     * @param category      分类，取值见 submittableCategories()
     * @param videoDuration 视频总时长（秒），用于服务端做范围校验
     * @param callback      (是否成功, 失败原因)
     */
    void submit(const std::string& bvid, int64_t cid, double start, double end, const std::string& category,
                double videoDuration, std::function<void(bool, SponsorSubmitError)> callback);

    /// 允许提交的分类（顺序即提交对话框里下拉框的顺序）
    static const std::vector<std::string>& submittableCategories();

    /**
     * 提交用的匿名用户 ID（36 位随机字符）。
     * 首次调用时生成并写入配置，之后保持不变；
     * 与 BilibiliSponsorBlock 的做法一致，不包含任何可识别用户的信息。
     */
    static std::string userID();

    /// 分类对应的进度条标记颜色，返回 0xRRGGBB
    static uint32_t colorForCategory(const std::string& category);

    /// 分类的中文名称，用于 OSD 提示
    static std::string categoryName(const std::string& category);

    /// 总开关，由配置项初始化
    inline static bool ENABLED = true;

    /// 是否在播放进度条上绘制分段颜色标记
    inline static bool SHOW_ON_PROGRESS_BAR = true;

    /// 服务端地址（BilibiliSponsorBlock 官方后端）
    inline static std::string SERVER = "https://www.bsbsb.top";

    /// 参与自动跳过的分类。
    /// 默认只开启语义明确、误跳风险低的三类（与上游 BilibiliSponsorBlock 的
    /// 默认行为对齐：上游仅把 sponsor 设为自动跳过，selfpromo / interaction
    /// 为手动跳过，preview 仅显示浮层）。
    /// 需要更多分类时，通过配置项 player_sponsor_categories 以逗号分隔追加，
    /// 可选值：sponsor, selfpromo, interaction, intro, outro, preview,
    ///        filler, music_offtopic, poi_highlight, exclusive_access
    inline static std::vector<std::string> CATEGORIES = {"sponsor", "intro", "outro"};

    SponsorBlock(const SponsorBlock&)            = delete;
    SponsorBlock& operator=(const SponsorBlock&) = delete;

private:
    SponsorBlock() = default;

    /// 发起异步请求
    void requestSegments(const std::string& hashPrefix, const std::string& bvid);

    /// 解析响应并过滤出当前视频的分段
    void applySegments(const std::string& body, const std::string& bvid);

    /// 提交成功后把新分段并入本地列表（仅在仍是同一个视频时生效）
    void addLocalSegment(const std::string& bvid, const SponsorSegment& seg);

    /// 生成 36 位随机字符串，用于 userID 与本地分段 UUID
    static std::string randomId(size_t length);

    std::mutex mtx;

    /// 当前正在播放的视频 ID
    std::string currentBvid;

    std::vector<SponsorSegment> segments;

    /// 与 segments 一一对应，标记该分段是否已跳过，避免反复跳转
    std::vector<bool> skipped;

    /// 分段集合的版本号，供 UI 感知数据变化
    uint64_t version = 0;

    /// 上次跳转的时间点，用于跳转冷却，避免与手动拖动进度条打架
    double lastSeekAt = -100;
};

}  // namespace wiliwili
