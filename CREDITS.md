# 致谢与第三方引用

本仓库是基于 **wiliwili** 的二次开发分支，新增了 SponsorBlock（空降助手，采用官方同款分段配色）、
进度条分段着色、B 站 CDN 优化（彻底屏蔽 PCDN / MCDN）、稍后再看一键加入/移除等功能。

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

### 4. B 站 PCDN / MCDN 域名清单来源

- 参考仓库：[Li-Dong-Don/AdGuard-BiliCDN-Rules](https://github.com/Li-Dong-Don/AdGuard-BiliCDN-Rules)
  与社区（AdGuard / Clash 规则集、B 站播放优化类博客）公开的实测数据。
- 引用方式：**仅参考其公开的域名清单作为屏蔽目标**，代码为独立实现（纯 C++ 字符串匹配，
  未引入任何 DNS 代理或外部过滤组件）。

本仓库内置屏蔽的 PCDN / MCDN 供应商：

| 供应商 | 域名 | 典型端口 |
|---|---|---|
| 京东云无线宝（MCDN） | `*.mcdn.bilivideo.cn`、`*.mcdn.bilivideo.com` | 8082 |
| B 站 pcdn 前缀域名 | `*.pcdn.bilivideo.cn`、`*.pcdn.bilivideo.com` | — |
| 迅雷 / 网心云 | `*.edge.mountaintoys.cn`、`*.xycdn.com`、`*.onethingpcs.com` | 4483 |
| 节点之家（深圳百达云） | `*.szbdyd.com` | 9305 |
| 派欧云 PPIO | `*.nexusedgeio.com`、`*.ppio.cloud` | 任意 |
| 京东云 CDN | `*.jdcloudcdn.com` | — |
| 兜底规则 | 主机名含 `pcdn` / `mcdn` / `p2p`；或使用非 80/443 端口的非官方域名（含裸 IP） | 任意 |

> 兜底规则用于覆盖后续新出现的 PCDN 供应商，无需等待清单更新。

### 5. Android 移植参考

Android 支持**不是**本仓库发明的，而是 borealis 自带的能力。以下项目提供了
可直接复用的信息与预编译产物：

| 项目 | 许可证 | 引用方式 |
|---|---|---|
| [xfangfang/borealis](https://github.com/xfangfang/borealis) 的 `android-project/` | Apache-2.0 | 直接作为模板来源：`AndroidManifest.xml`、`gradlew` / `gradle-wrapper`、`proguard-rules.pro`、`org.libsdl.app.*` 这 11 个 Java 文件、各密度 `ic_launcher.png` **逐字节复制**后按 wiliwili 需要改写 |
| [Tenwang318/wiliwili-android](https://github.com/Tenwang318/wiliwili-android) | GPL-3.0 | 参考其 `android-project/app/jni/CMakeLists.txt`、`app/build.gradle`、`.ci/prepare_mpv.sh`、`.github/workflows/android.yml` 的构建参数（NDK 版本、ABI 列表、mbedtls 接法、16 KB 页对齐）。**未复制其代码，参数已按本仓库情况重写并补充注释** |
| [jarnedemeulemeester/libmpv-android](https://github.com/jarnedemeulemeester/libmpv-android) | LGPL-2.1+ | 运行时依赖：`.ci/prepare_mpv.sh` 下载其 v1.0.0 预编译 AAR，解出各 ABI 的 `libmpv.so` 与 FFmpeg 系列 `.so` 打包进 APK |
| [mpv-player/mpv](https://github.com/mpv-player/mpv) v0.41.0 | LGPL-2.1+ / GPL-2.0 | 构建期依赖：仅取其 `include/mpv/*.h` 头文件用于编译 |
| [Mbed-TLS/mbedtls](https://github.com/Mbed-TLS/mbedtls) 3.5.2 | Apache-2.0 | 构建期依赖：NDK 无 OpenSSL，由 `jni/CMakeLists.txt` 经 `FetchContent` 编成静态库作为 cpr/curl 的 TLS 后端 |
| [Android NDK](https://developer.android.com/ndk) r26.1.10909125 | — | 交叉编译工具链 |
| [SDL2](https://github.com/libsdl-org/SDL)（borealis 内置） | Zlib | Android 窗口 / 输入 / GLES 上下文，手柄与遥控器支持的实际实现者 |

本仓库对上述内容的**增量改动**（均为 AI 生成）：

- `WiliwiliActivity.java`：新增 `dispatchKeyEvent` 按键改写（遥控器媒体键、
  以及非摇杆类遥控器的 OK 键），见 `android-project/README.md`
- `AndroidManifest.xml`：新增 `LEANBACK_LAUNCHER`、`android.software.leanback`、
  `android.hardware.usb.host` / `bluetooth` 声明、`sensorLandscape` 与 `tv_banner`
- `res/drawable-xhdpi/tv_banner.png`：为 Android TV 首页新绘制的 320×180 横幅
- `app/build.gradle`：包名 / 版本号改为本仓库，新增 `-DCPR_FORCE_MBEDTLS_BACKEND=ON`

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

**Android 平台新增文件（工程共 30 个文件）**

| 类别 | 文件 |
|---|---|
| 构建配置 | `android-project/build.gradle`、`settings.gradle`、`gradle.properties`、`gradle/wrapper/*`、`gradlew`、`gradlew.bat`、`app/build.gradle`、`app/proguard-rules.pro` |
| 原生构建 | `android-project/app/jni/CMakeLists.txt` |
| 应用入口 | `android-project/app/src/main/java/cn/xfangfang/wiliwili/WiliwiliActivity.java` |
| 清单 / 资源 | `AndroidManifest.xml`、`res/values/*.xml`、`res/mipmap-*/ic_launcher*.png`、`res/mipmap-anydpi-v26/ic_launcher.xml`、`res/drawable-xhdpi/tv_banner.png` |
| SDL Java 层 | `res` 同级的 `java/org/libsdl/app/*.java`（11 个，自 borealis 模板逐字节复制） |
| CI / 脚本 | `.ci/prepare_mpv.sh`、`.github/workflows/android.yml` |
| 文档 | `android-project/README.md` |

**修改文件（28 个）**

| 类别 | 文件 |
|---|---|
| 功能接入 | `player_base_activity.cpp`、`player_activity.{hpp,cpp}`、`player_activity.xml`、`video_view.{hpp,cpp}`、`video_progress_slider.{hpp,cpp}` |
| CDN / PCDN | `video_detail_api.cpp`（普通视频 + 番剧 + 投屏）、`live_player_activity.cpp`（直播） |
| 稍后再看 API | `bilibili/api.h`、`bilibili.h`、`mine_api.cpp` |
| 配置项 | `config_helper.{hpp,cpp}`（新增 4 个配置键） |
| 设置界面 | `setting_activity.{hpp,cpp}`、`setting_activity.xml` |
| 多语言 | 7 种语言的 `wiliwili.json` |
| 构建 / CI | `CMakeLists.txt`、`.github/workflows/release-cn.yml`、`.github/workflows/android.yml`、`.gitignore` |

### 已知局限

AI 生成的代码存在以下**未验证项**：

- 未做真机 GUI 启动测试（仅通过静态依赖校验）
- 进度条着色未在真实播放中肉眼确认
- PCDN 过滤规则已用 26 条离线用例验证（`_research/verify_pcdn.cpp`），
  但未接入真实 playurl 响应做端到端验证
- **Android 版未在真机 / 模拟器上运行过**：仅通过 68 项静态校验
  （YAML / XML / Java 语法 / 构建接线，见 `_research/verify_android_project.py`）
  与 CI 构建产物校验（三个 ABI 的 `.so` 齐备、16 KB 页对齐）
- 手柄与遥控器的按键映射结论来自对 `sdl_input.cpp`、
  `SDLControllerManager.java`、`SDL_androidkeyboard.c` 的**源码走查**，
  未做真实设备实测

使用前请自行评估。

---

## 四、许可证

本仓库继承上游 wiliwili 的 **GPL-3.0** 许可证，详见 [LICENSE](LICENSE)。
