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

#pragma once

#include <cstdint>
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
