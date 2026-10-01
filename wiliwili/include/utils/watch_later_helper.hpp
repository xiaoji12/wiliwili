//
// 稍后再看（Watch Later）本地状态助手
//
// B 站的「稍后再看」是服务端状态，视频详情接口并不会返回「是否已加入」，
// 因此需要在客户端维护一份 bvid 集合缓存：
//   - 进入播放页时按需拉取一次列表（带冷却，避免每个视频都请求）
//   - 加入 / 移除成功后即时更新缓存，让按钮状态立刻正确
//
// 所有对外回调都会被切回主线程（brls::sync），调用方可以直接刷新 UI。
//

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <string>

namespace wiliwili {

class WatchLaterHelper {
public:
    /// 是否在播放页显示「稍后再看」按钮
    inline static bool SHOW_BUTTON = true;

    static WatchLaterHelper& instance();

    /// 本地缓存中是否已加入稍后再看
    bool contains(const std::string& bvid);

    /// 是否已经成功拉取过一次列表
    bool loaded();

    /**
     * 从服务端刷新缓存。
     * @param done  刷新结束（无论成功失败）在主线程回调，可为空
     * @param force true 时忽略冷却时间强制刷新
     */
    void refresh(const std::function<void()>& done = nullptr, bool force = false);

    /**
     * 加入稍后再看。
     * @param done 结果在主线程回调；true = 成功
     */
    void add(const std::string& bvid, uint64_t aid, const std::function<void(bool)>& done = nullptr);

    /**
     * 从稍后再看移除。
     * @param done 结果在主线程回调；true = 成功
     */
    void remove(const std::string& bvid, uint64_t aid, const std::function<void(bool)>& done = nullptr);

private:
    WatchLaterHelper() = default;

    std::mutex mtx;
    std::set<std::string> cachedBvids;
    bool hasLoaded = false;
    std::chrono::steady_clock::time_point lastRefreshAt{};
};

}  // namespace wiliwili
