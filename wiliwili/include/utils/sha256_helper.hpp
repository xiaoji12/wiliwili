//
// SHA-256 摘要工具（空降助手 / SponsorBlock 模块使用）
//
// SponsorBlock 协议要求只发送 SHA256(bvid) 的前 4 位十六进制，
// 服务端返回该哈希桶下的全部视频分段，客户端再按 bvid 本地过滤，
// 以此避免向第三方泄露用户观看的完整视频 ID。
//
// 工程内原本没有 SHA-256 实现，这里提供一份自包含的版本，
// 避免引入 OpenSSL 带来的各平台（Switch / PSV / PS4）移植负担。
//

#pragma once

#include <string>

namespace wiliwili {

/**
 * 计算字符串的 SHA-256 摘要
 * @param input 待计算的原始字节
 * @return 64 个小写十六进制字符
 */
std::string sha256Hex(const std::string& input);

}  // namespace wiliwili
