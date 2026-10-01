//
// B 站播放地址（CDN）优化
//
// B 站 playurl 接口会为同一个视频返回多组播放地址：
//   - durl[].url       主地址
//   - durl[].backup_url 备用地址列表
//   - dash.video/audio[].base_url + backup_url
//
// 其中一部分地址指向 PCDN（P2P CDN）节点。这类节点由其他用户的设备提供带宽，
// 特点是速度抖动大、首帧慢，并且会占用本机上行带宽。屏蔽它们可以明显改善体验。
//
// 本模块提供两个能力：
//   1. 屏蔽 PCDN 地址（当存在正常 CDN 地址时）
//   2. 按运营商偏好把指定 CDN 的地址排到最前
//

#pragma once

#include <string>
#include <vector>

namespace bilibili {

class CDNHelper {
public:
    /// 是否屏蔽 PCDN 节点
    inline static bool BLOCK_PCDN = true;

    /**
     * CDN 优选。
     * 0 = 自动（不干预顺序）
     * 1 = 阿里云   2 = 腾讯云   3 = 华为云   4 = 火山引擎   5 = 百度云
     */
    inline static int PREFER = 0;

    /// 判断该地址是否指向 PCDN 节点
    static bool isPcdn(const std::string& url);

    /// 判断该地址是否属于指定厂商的 CDN
    static bool matchCdn(const std::string& url, int prefer);

    /**
     * 对一组地址应用「屏蔽 PCDN + CDN 优选」。
     * 若过滤后一个地址都不剩，则原样返回，避免把视频彻底变成不可播放。
     */
    static std::vector<std::string> filter(const std::vector<std::string>& urls);
};

}  // namespace bilibili
