# 致谢与第三方引用

本仓库是基于 **wiliwili** 的二次开发分支，新增了 SponsorBlock（空降助手，采用官方同款分段配色）、
进度条分段着色、B 站 CDN 优化（屏蔽 PCDN）、稍后再看一键加入/移除等功能。

> **本仓库的全部新增与修改代码均由 AI 生成**，详见文末 [AI 声明](#三ai-声明)。

---

## 一、直接参考的项目

### 1. xfangfang/wiliwili —— 上游项目

- 仓库：https://github.com/xfangfang/wiliwili
- 许可证：GPL-3.0
- 引用方式：**本仓库是它的 fork**，完整保留了其原始代码与资源。
  新增功能以补丁形式叠加，未修改任何第三方子模块。

### 2. hanydd/BilibiliSponsorBlock —— 协议与数据来源

- 仓库：https://github.com/hanydd/BilibiliSponsorBlock
- 引用方式：**空降助手功能完全遵循该项目定义的客户端协议**：
  - 接口：`GET {SERVER}/api/skipSegments/{SHA256(videoID) 前 4 位十六进制}`
  - 响应：裸 JSON 数组，一个哈希桶内含多个视频，客户端按 `videoID` 本地过滤
  - 无数据时返回 HTTP 404
  - 分类名沿用其定义：`sponsor` / `selfpromo` / `interaction` / `intro` / `outro` /
    `preview` / `filler` / `music_offtopic` / `poi_highlight` / `exclusive_access`
  - 默认自动跳过的分类对齐其上游行为（上游默认仅 `sponsor` 为自动跳过）
- 默认服务端：`https://www.bsbsb.top`（该项目对应的社区后端）

### 3. ajayyy/SponsorBlock —— 原始协议与配色

- 仓库：https://github.com/ajayyy/SponsorBlock
- 引用方式：
  - **进度条分段配色完全照搬其官方浏览器扩展的 `barTypes[*].color`**，
    使客户端与网页端颜色一致（赞助 `#00D400` 绿 / 自我推广 `#FFFF00` 黄 /
    互动 `#CC00FF` 紫 / 开场 `#00FFFF` 青 / 结尾 `#0202ED` 蓝 /
    预告 `#008FD6` 浅蓝 / 离题 `#7300FF` 紫罗兰 / 非音乐部分 `#FF9900` 橙 /
    高能时刻 `#FF1684` 粉 / 独占内容 `#008A5C` 深绿）
  - 「只发送视频 ID 哈希前缀以保护隐私」的设计思路亦源自该项目

---

## 二、上游 wiliwili 依赖的第三方库

以下库由上游项目引入，本仓库**未做任何改动**，一并致谢：

| 项目 | 用途 | 许可证 |
|---|---|---|
| [borealis](https://github.com/xfangfang/borealis) | 跨平台 UI 框架 | Apache-2.0 |
| [mpv](https://mpv.io) / libmpv | 视频播放内核 | GPL-2.0 / LGPL-2.1 |
| [FFmpeg](https://www.ffmpeg.org) | 音视频编解码 | LGPL-2.1+ |
| [cpr](https://github.com/libcpr/cpr) | HTTP 客户端（libcurl 封装） | MIT |
| [curl](https://curl.se) | 网络传输 | curl 许可证 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 解析 | MIT |
| [OpenCC](https://github.com/BYVoid/OpenCC) | 简繁转换 | Apache-2.0 |
| [pystring](https://github.com/imageworks/pystring) | 字符串工具 | BSD-3-Clause |
| [lunasvg](https://github.com/sammycage/lunasvg) | SVG 渲染 | MIT |
| [GLFW](https://www.glfw.org) | 窗口与输入 | Zlib |
| [fmt](https://github.com/fmtlib/fmt) | 字符串格式化 | MIT |
| [mongoose](https://github.com/cesanta/mongoose) | 内嵌 Web 服务（DLNA） | GPL-2.0 |
| [QR-Code-generator](https://github.com/nayuki/QR-Code-generator) | 二维码生成 | MIT |
| [libwebp](https://chromium.googlesource.com/webm/libwebp) | WebP 图片解码 | BSD-3-Clause |
| [yoga](https://github.com/facebook/yoga) | Flex 布局引擎 | MIT |
| [marisa-trie](https://github.com/s-yata/marisa-trie) | 弹幕关键词过滤 | BSD-2-Clause / LGPL-2.1 |
| [tinyxml2](https://github.com/leethomason/tinyxml2) | XML 解析 | Zlib |
| [pdr](https://github.com/xfangfang/pdr) | 弹幕渲染 | MIT |

---

## 三、AI 声明

### 本仓库新增与修改的代码 100% 由 AI 生成

| 项 | 内容 |
|---|---|
| 生成工具 | WorkBuddy AI |
| 生成时间 | 2026-10-01 |
| 人工介入 | 仅提出需求与验收，**未手写任何代码** |

### 具体由 AI 生成的内容

**新增文件（9 个）**

| 文件 | 说明 |
|---|---|
| `wiliwili/include/api/sponsor_block.hpp` | 空降助手接口定义 |
| `wiliwili/source/api/sponsor_block.cpp` | 空降助手实现（请求 / 解析 / 跳过 / 官方配色） |
| `wiliwili/include/utils/sha256_helper.hpp` | SHA-256 接口 |
| `wiliwili/source/utils/sha256_helper.cpp` | 自包含 SHA-256 实现（FIPS 180-4） |
| `wiliwili/include/utils/cdn_helper.hpp` | CDN 优化接口 |
| `wiliwili/source/utils/cdn_helper.cpp` | PCDN 屏蔽 + CDN 优选实现 |
| `wiliwili/include/utils/watch_later_helper.hpp` | 稍后再看状态缓存接口 |
| `wiliwili/source/utils/watch_later_helper.cpp` | 稍后再看加入 / 移除 / 列表缓存实现 |
| `resources/svg/bpx-svg-sprite-later(-active).svg` | 稍后再看按钮图标（普通 / 激活态） |

**修改文件（26 个）**

| 类别 | 文件 |
|---|---|
| 功能接入 | `player_base_activity.cpp`、`player_activity.{hpp,cpp}`、`player_activity.xml`、`video_view.{hpp,cpp}`、`video_progress_slider.{hpp,cpp}`、`video_detail_api.cpp` |
| 稍后再看 API | `bilibili/api.h`、`bilibili.h`、`mine_api.cpp` |
| 配置项 | `config_helper.{hpp,cpp}`（新增 4 个配置键） |
| 设置界面 | `setting_activity.{hpp,cpp}`、`setting_activity.xml` |
| 多语言 | 7 种语言的 `wiliwili.json` |
| 构建 / CI | `CMakeLists.txt`、`.github/workflows/release-cn.yml` |

### 已知局限

AI 生成的代码存在以下**未验证项**：

- 未做真机 GUI 启动测试（仅通过静态依赖校验）
- 进度条着色未在真实播放中肉眼确认
- PCDN 过滤未用真实 playurl 响应验证

使用前请自行评估。

---

## 四、许可证

本仓库继承上游 wiliwili 的 **GPL-3.0** 许可证，详见 [LICENSE](LICENSE)。
