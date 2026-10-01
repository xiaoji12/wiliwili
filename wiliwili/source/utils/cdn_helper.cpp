//
// B 站播放地址（CDN）优化实现
//

#include "utils/cdn_helper.hpp"

#include <algorithm>

namespace bilibili {

namespace {

/// 判断 host 部分是否包含某个子串
inline bool has(const std::string& url, const char* needle) {
    return url.find(needle) != std::string::npos;
}

/// 提取 URL 中的主机名（去掉协议、路径、端口）
std::string hostOf(const std::string& url) {
    std::string s = url;
    const auto scheme = s.find("://");
    if (scheme != std::string::npos) s = s.substr(scheme + 3);
    const auto slash = s.find('/');
    if (slash != std::string::npos) s = s.substr(0, slash);
    const auto colon = s.find(':');
    if (colon != std::string::npos) s = s.substr(0, colon);
    return s;
}

}  // namespace

bool CDNHelper::isPcdn(const std::string& url) {
    if (url.empty()) return false;

    const std::string host = hostOf(url);

    // B 站移动 PCDN（形如 mcdn.bilivideo.cn:8082）
    if (host.find("mcdn.bilivideo") != std::string::npos) return true;

    // 深圳百达云，B 站主要的 PCDN 供应商
    if (host.find("szbdyd.com") != std::string::npos) return true;

    // 显式的 PCDN 标记
    if (host.find("pcdn") != std::string::npos) return true;

    return false;
}

bool CDNHelper::matchCdn(const std::string& url, int prefer) {
    if (url.empty() || prefer <= 0) return false;

    const std::string host = hostOf(url);

    switch (prefer) {
        case 1:  // 阿里云
            return has(host, "mirrorali") || has(host, "aliyuncs") || has(host, "alicdn");
        case 2:  // 腾讯云
            return has(host, "mirrorcos") || has(host, "myqcloud") || has(host, "tencentcs");
        case 3:  // 华为云
            return has(host, "mirrorhw") || has(host, "myhuaweicloud") || has(host, "hwcdn");
        case 4:  // 火山引擎 / 字节
            return has(host, "mirror08c") || has(host, "volces") || has(host, "bytedance");
        case 5:  // 百度云
            return has(host, "mirrorbd") || has(host, "bcebos") || has(host, "baidubce");
        default:
            return false;
    }
}

std::vector<std::string> CDNHelper::filter(const std::vector<std::string>& urls) {
    if (urls.empty()) return urls;

    std::vector<std::string> normal;
    normal.reserve(urls.size());
    bool hasPcdn = false;

    for (const auto& u : urls) {
        if (u.empty()) continue;
        if (isPcdn(u)) {
            hasPcdn = true;
        } else {
            normal.emplace_back(u);
        }
    }

    std::vector<std::string> result;

    if (BLOCK_PCDN) {
        // 全部都是 PCDN 时不做过滤，否则视频会彻底无法播放
        result = normal.empty() ? urls : normal;
    } else {
        result = urls;
    }

    if (!hasPcdn && PREFER <= 0) return result;

    // CDN 优选：命中的地址稳定地排到最前，其余保持原有相对顺序
    if (PREFER > 0) {
        const int prefer = PREFER;
        std::stable_sort(result.begin(), result.end(),
                         [prefer](const std::string& a, const std::string& b) {
                             return matchCdn(a, prefer) && !matchCdn(b, prefer);
                         });
    }

    return result;
}

}  // namespace bilibili
