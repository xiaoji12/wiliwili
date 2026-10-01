//
// B 站播放地址（CDN）优化实现
//
// PCDN（P2P CDN）节点由其他用户的家庭宽带提供带宽，首帧慢、抖动大，
// 且会占用本机上行带宽。B 站会在 playurl 里混入这类地址，这里做统一识别与剔除。
//
// 识别策略（按优先级）：
//   1. 已知 PCDN / MCDN 供应商域名后缀 —— 命中即屏蔽
//   2. 主机名里带 pcdn / mcdn / p2p 标记
//   3. 非标准端口（官方 CDN 一律走 80 / 443，PCDN 才会用 8082 / 4483 / 9305 之类）
//      但官方镜像（upos-* / mirror*）与正规云厂商域名不套用此规则，避免误杀
//
// 已知供应商对照表：
//   京东云无线宝      *.mcdn.bilivideo.cn:8082   *.mcdn.bilivideo.com
//   迅雷 / 网心云     *.edge.mountaintoys.cn:4483   *.xycdn.com   *.onethingpcs.com
//   节点之家(百达云)   *.szbdyd.com:9305
//   派欧云 PPIO       *.nexusedgeio.com   *.ppio.cloud
//   京东云 CDN        *.jdcloudcdn.com
//

#include "utils/cdn_helper.hpp"

#include <algorithm>
#include <cctype>

namespace bilibili {

namespace {

/// 判断字符串是否包含某个子串
inline bool has(const std::string& s, const char* needle) {
    return s.find(needle) != std::string::npos;
}

/// 转小写（域名大小写不敏感）
std::string lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/// 去掉协议与路径，保留 host[:port]
std::string authorityOf(const std::string& url) {
    std::string s = url;
    const auto scheme = s.find("://");
    if (scheme != std::string::npos) s = s.substr(scheme + 3);
    const auto slash = s.find('/');
    if (slash != std::string::npos) s = s.substr(0, slash);
    const auto question = s.find('?');
    if (question != std::string::npos) s = s.substr(0, question);
    // 去掉可能存在的 userinfo@
    const auto at = s.rfind('@');
    if (at != std::string::npos) s = s.substr(at + 1);
    return s;
}

/// 提取 URL 中的主机名（去掉协议、路径、端口、userinfo）
std::string hostOf(const std::string& url) {
    std::string s = authorityOf(url);
    // IPv6 字面量：[::1]:8080
    if (!s.empty() && s.front() == '[') {
        const auto close = s.find(']');
        if (close != std::string::npos) return s.substr(1, close - 1);
        return s;
    }
    const auto colon = s.rfind(':');
    if (colon != std::string::npos) s = s.substr(0, colon);
    return s;
}

/// 提取端口；没有显式端口时返回 0
int portOf(const std::string& url) {
    std::string s = authorityOf(url);
    if (!s.empty() && s.front() == '[') {
        const auto close = s.find(']');
        if (close == std::string::npos) return 0;
        s = s.substr(close + 1);
        if (s.empty() || s[0] != ':') return 0;
        s = s.substr(1);
    } else {
        const auto colon = s.rfind(':');
        if (colon == std::string::npos) return 0;
        s = s.substr(colon + 1);
    }
    if (s.empty()) return 0;
    int port = 0;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return 0;
        port = port * 10 + (c - '0');
        if (port > 65535) return 0;
    }
    return port;
}

/// 域名是否等于 suffix 或为其子域
bool matchSuffix(const std::string& host, const char* suffix) {
    const std::string suf(suffix);
    if (host == suf) return true;
    if (host.size() > suf.size() + 1 &&
        host.compare(host.size() - suf.size() - 1, suf.size() + 1, "." + suf) == 0)
        return true;
    return false;
}

/// 已知的 PCDN / MCDN 供应商域名（命中即屏蔽，优先级最高）
bool isKnownPcdnHost(const std::string& host) {
    static const char* kPcdnHosts[] = {
        // 京东云无线宝 MCDN
        "mcdn.bilivideo.cn",
        "mcdn.bilivideo.com",
        // B 站 pcdn 前缀域名
        "pcdn.bilivideo.cn",
        "pcdn.bilivideo.com",
        // 迅雷 / 网心云
        "mountaintoys.cn",
        "xycdn.com",
        "onethingpcs.com",
        // 节点之家（深圳百达云）
        "szbdyd.com",
        // 派欧云 PPIO
        "nexusedgeio.com",
        "ppio.cloud",
        // 京东云 CDN
        "jdcloudcdn.com",
    };
    for (const char* suffix : kPcdnHosts) {
        if (matchSuffix(host, suffix)) return true;
    }
    return false;
}

/// 官方 / 正规 CDN 主机：不套用「非标准端口」启发式，避免误杀
bool isOfficialCdnHost(const std::string& host) {
    // B 站自建镜像一律以 upos- 开头，如 upos-sz-mirrorali.bilivideo.com
    if (host.rfind("upos-", 0) == 0) return true;
    if (has(host, "mirror")) return true;

    static const char* kGoodHosts[] = {
        "hdslb.com",         "biliapi.net",    "bilibili.com",
        "akamaized.net",     "akamaiedge.net", "akamaihd.net",
        "cloudfront.net",    "aliyuncs.com",   "myqcloud.com",
        "myhuaweicloud.com", "volces.com",     "bcebos.com",
    };
    for (const char* suffix : kGoodHosts) {
        if (matchSuffix(host, suffix)) return true;
    }
    return false;
}

/// 官方 CDN 惯用的标准端口
inline bool isStandardPort(int port) { return port == 0 || port == 80 || port == 443; }

}  // namespace

bool CDNHelper::isPcdnHost(const std::string& host) {
    if (host.empty()) return false;

    const std::string h = lower(host);

    // 1. 已知 PCDN 供应商
    if (isKnownPcdnHost(h)) return true;

    // 2. 显式的 PCDN / MCDN / P2P 标记
    if (has(h, "pcdn") || has(h, "mcdn") || has(h, "p2p")) return true;

    return false;
}

bool CDNHelper::isPcdn(const std::string& url) {
    if (url.empty()) return false;

    const std::string host = hostOf(url);
    if (host.empty()) return false;

    // 1 / 2：域名层面的判定
    if (isPcdnHost(host)) return true;

    // 3：非标准端口。官方 CDN 一律 80 / 443，PCDN 才会用 8082 / 4483 / 9305 之类。
    //    官方镜像（upos-* / mirror*）与正规云厂商域名不受此规则约束。
    const std::string h = lower(host);
    if (!isOfficialCdnHost(h) && !isStandardPort(portOf(url))) return true;

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
