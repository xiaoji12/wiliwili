<p align="center">
    <img src="resources/svg/cn.xfangfang.wiliwili.svg" alt="logo" height="128" width="128"/>
</p>
<p align="center">
  一个专为手柄用户设计的第三方 <a href="https://www.bilibili.com">B站</a> 客户端
</p>
<p align="center">
<b><a href="#特点">特点</a></b>
|
<b><a href="#本分支新增功能">本分支新增功能</a></b>
|
<b><a href="#安装">安装</a></b>
|
<b><a href="#开发">开发</a></b>
</p>

- - -

![MS](https://img.shields.io/badge/-Windows%207+-357ec7?style=flat&logo=Windows)
![Linux](https://img.shields.io/badge/-Linux-lightgrey?style=flat&logo=Linux&logoColor=white)
![Android](https://img.shields.io/badge/-Android%208.0+-3DDC84?style=flat&logo=Android&logoColor=white)

<br>

# 特点

wiliwili 拥有非常接近官方PC客户端的B站浏览体验  
同时支持**触屏**、**鼠标**、**键盘** 与 **手柄**操控  
无论是电脑还是游戏掌机都能获得全新的使用体验

多语言：简、繁、日、韩、英 ...   
搜索页：热搜 视频 番剧 影视  
筛选页：快速找到想看的影视内容  
动态页：关注的UP主最近视频动态  
直播页：关注的主播与其他系统推荐  
播放页：视频 番剧 电影 纪录片 综艺，支持弹幕与评论  
个人页：扫码登录 历史记录 个人收藏 我的追番 我的追剧  
主题色：拥有深浅两色主题，跟随系统自动切换

<br>

# 本分支新增功能

- **SponsorBlock 空降助手**：播放时自动跳过社区标注的赞助 / 片头 / 片尾等片段，分类配色与官方 SponsorBlock 一致
- **进度条分段着色**：进度条上按分类标出各标注片段
- **B 站 CDN 优化**：支持按运营商优选 CDN，并彻底屏蔽 PCDN / MCDN 节点
- **稍后再看**：播放页一键加入 / 移除
- **Android 版**：手柄与 Android TV 遥控器可用

引用清单与已知局限见 **[CREDITS.md](CREDITS.md)**。

<br>

# 安装

下载地址：[本仓库 Releases](https://github.com/xiaoji12/wiliwili/releases)

| 平台 | 文件 | 说明 |
| --- | --- | --- |
| Windows x86_64 | `wiliwili-Windows-*-portable.zip` | 绿色便携版，解压即用，已内置全部运行库（含 libmpv） |
| Linux / Steam Deck x86_64 | `wiliwili-Linux-*-x86_64.flatpak` | Flatpak 包，Steam Deck 桌面模式可直接安装 |
| Android 8.0+ | `wiliwili-Android-*.apk` | 通用 APK，含 arm64-v8a / armeabi-v7a / x86_64 |

### Windows

解压后双击 `wiliwili.exe` 即可，无需安装任何运行库。

### Linux / Steam Deck

```bash
# 方式一：命令行
flatpak install --user ./wiliwili-Linux-*-x86_64.flatpak

# 方式二：桌面模式双击 .flatpak 文件，由 Discover / GNOME Software 安装
```

装好后可直接在桌面模式启动；若想在游戏模式使用，可在 Steam 中「添加非 Steam 游戏」指向 `flatpak run cn.xfangfang.wiliwili`。

### Android

一个通用 APK 同时包含 `arm64-v8a` / `armeabi-v7a` / `x86_64` 三个 ABI，最低 Android 8.0（API 26）。

- **手柄**：有线（USB）/ 蓝牙手柄即插即用，方向键、ABXY、肩键、摇杆、扳机、震动全部可用
- **Android TV 遥控器**：方向键、OK、返回、菜单键可用；播放/暂停、快进、快退已映射到 wiliwili 的默认快捷键
- **Android TV**：声明了 `LEANBACK_LAUNCHER` 与 TV 横幅，会出现在电视首页
- 手机 / 平板同样可以安装，触屏与手柄可混用

详细说明（按键映射表、构建方法、为什么锁 NDK r26、16 KB 页对齐等）
见 **[android-project/README.md](android-project/README.md)**。

<br>

# 开发

```shell
# 拉取代码（含子模块）
git clone --recursive https://github.com/xiaoji12/wiliwili.git
cd wiliwili
```

### PC 本地运行

目前本分支在 Linux 与 Windows 上验证通过。

<details>

#### Linux

不同 Linux 的编译过程或依赖可能不同，这里是一份总结：[#89](https://github.com/xfangfang/wiliwili/discussions/89)

```shell
# Ubuntu: install dependencies
sudo apt install libssl-dev libmpv-dev libwebp-dev

cmake -B build -DPLATFORM_DESKTOP=ON
make -C build wiliwili -j$(nproc)
```

```shell
# 如果你想安装在系统路径，并生成一个桌面图标，请使用如下内容编译
cmake -B build -DPLATFORM_DESKTOP=ON -DINSTALL=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=/usr
make -C build wiliwili -j$(nproc)
sudo make -C build install

# uninstall (run after install)
sudo xargs -a build/install_manifest.txt rm
```

#### Windows

```shell
# Windows: install dependencies (MSYS2 MinGW64)
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make \
  git mingw-w64-x86_64-mpv mingw-w64-x86_64-libwebp

cmake -B build -G "MinGW Makefiles" -DPLATFORM_DESKTOP=ON
mingw32-make -C build wiliwili -j$(nproc)
```

</details>

### Android

Android 版由 `.github/workflows/android.yml` 在 GitHub Actions 上构建，
本地构建需要 Android SDK + **NDK r26.1.10909125** + CMake 3.22.1。
完整步骤与踩坑说明见 **[android-project/README.md](android-project/README.md)**。

<br>

# 应用截图

<p align="center">
<img src="docs/images/screenshot-3.jpg" alt="screenshot">
<img src="docs/images/screenshot-4.jpg" alt="screenshot">
</p>

# Acknowledgement

The development of wiliwili cannot do without the support of the following organization and open source projects.

- UI Library: natinusala and XITRIX
    - https://github.com/natinusala/borealis
    - https://github.com/XITRIX/borealis
- Video Player: Cpasjuste, proconsule, fish47 and averne
    - https://github.com/Cpasjuste/pplay
    - https://github.com/proconsule/nxmp
    - https://github.com/fish47/FFmpeg-vita
    - https://github.com/fish47/mpv-vita
    - http://github.com/averne/FFmpeg
    - http://github.com/averne/mpv
- Misc
    - https://github.com/libcpr/cpr
    - https://github.com/nlohmann/json
    - https://github.com/nayuki/QR-Code-generator
    - https://github.com/BYVoid/OpenCC
    - https://github.com/imageworks/pystring
    - https://github.com/sammycage/lunasvg
    - https://github.com/cesanta/mongoose
    - https://chromium.googlesource.com/webm/libwebp
    - https://github.com/fancycode/MemoryModule

> 完整的第三方致谢清单（各平台工具链、播放器内核贡献者等）见 [上游项目 README](https://github.com/xfangfang/wiliwili#acknowledgement)。

# Special thanks

- Thanks to Crowdin for supporting [open-source projects](https://crowdin.com/page/open-source-project-setup-request).
