//
// 稍后再看（Watch Later）本地状态助手实现
//

#include "utils/watch_later_helper.hpp"

#include <borealis.hpp>

#include "bilibili.h"
#include "bilibili/result/mine_later_result.h"
#include "utils/config_helper.hpp"

namespace wiliwili {

/// 缓存冷却：同一段时间内不重复拉取列表
static constexpr int REFRESH_COOLDOWN_SEC = 120;

WatchLaterHelper& WatchLaterHelper::instance() {
    static WatchLaterHelper inst;
    return inst;
}

bool WatchLaterHelper::contains(const std::string& bvid) {
    if (bvid.empty()) return false;
    std::lock_guard<std::mutex> lock(mtx);
    return cachedBvids.count(bvid) > 0;
}

bool WatchLaterHelper::loaded() {
    std::lock_guard<std::mutex> lock(mtx);
    return hasLoaded;
}

void WatchLaterHelper::refresh(const std::function<void()>& done, bool force) {
    bool skip = false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        const auto now = std::chrono::steady_clock::now();
        const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - lastRefreshAt).count();
        skip           = (!force && hasLoaded && age < REFRESH_COOLDOWN_SEC);
    }

    // 注意：brls::sync 在主线程会直接执行，因此必须先把锁释放掉再调用，
    // 否则回调里再访问本对象会造成自死锁。
    if (skip) {
        if (done) brls::sync(done);
        return;
    }

    bilibili::BilibiliClient::getWatchLater(
        [this, done](const bilibili::WatchLaterListWrapper& result) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                cachedBvids.clear();
                for (const auto& item : result.list) {
                    if (!item.bvid.empty()) cachedBvids.insert(item.bvid);
                }
                hasLoaded     = true;
                lastRefreshAt = std::chrono::steady_clock::now();
            }
            if (done) brls::sync(done);
        },
        [done](const std::string& error, int code) {
            brls::Logger::warning("WatchLaterHelper: 拉取稍后再看列表失败({}): {}", code, error);
            if (done) brls::sync(done);
        });
}

void WatchLaterHelper::add(const std::string& bvid, uint64_t aid, const std::function<void(bool)>& done) {
    const std::string csrf = ProgramConfig::instance().getCSRF();
    if (csrf.empty()) {
        brls::Logger::warning("WatchLaterHelper: 未登录，无法加入稍后再看");
        if (done) brls::sync([done]() { done(false); });
        return;
    }

    bilibili::BilibiliClient::add_to_watch_later(
        csrf, aid,
        [this, bvid, done]() {
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (!bvid.empty()) cachedBvids.insert(bvid);
                hasLoaded = true;
            }
            brls::Logger::info("WatchLaterHelper: 已加入稍后再看 {}", bvid);
            if (done) brls::sync([done]() { done(true); });
        },
        [done](const std::string& error, int code) {
            brls::Logger::error("WatchLaterHelper: 加入稍后再看失败({}): {}", code, error);
            if (done) brls::sync([done]() { done(false); });
        });
}

void WatchLaterHelper::remove(const std::string& bvid, uint64_t aid, const std::function<void(bool)>& done) {
    const std::string csrf = ProgramConfig::instance().getCSRF();
    if (csrf.empty()) {
        brls::Logger::warning("WatchLaterHelper: 未登录，无法移除稍后再看");
        if (done) brls::sync([done]() { done(false); });
        return;
    }

    bilibili::BilibiliClient::remove_from_watch_later(
        csrf, aid,
        [this, bvid, done]() {
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (!bvid.empty()) cachedBvids.erase(bvid);
                hasLoaded = true;
            }
            brls::Logger::info("WatchLaterHelper: 已从稍后再看移除 {}", bvid);
            if (done) brls::sync([done]() { done(true); });
        },
        [done](const std::string& error, int code) {
            brls::Logger::error("WatchLaterHelper: 移除稍后再看失败({}): {}", code, error);
            if (done) brls::sync([done]() { done(false); });
        });
}

}  // namespace wiliwili
