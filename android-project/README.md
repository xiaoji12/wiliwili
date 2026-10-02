# wiliwili for Android

Android 版 wiliwili。基于 borealis 自带的 Android 后端（SDL2 + OpenGL ES 3 +
libromfs），**wiliwili 主体源码不需要为 Android 做任何改动**。

产物是一个包含 `armeabi-v7a` / `arm64-v8a` / `x86_64` 三个 ABI 的通用 APK。

## 手柄支持

全部由 borealis 的 SDL 后端提供
（`library/borealis/library/lib/platforms/sdl/sdl_input.cpp`），无需额外代码：

| 输入 | 实现 |
| --- | --- |
| DPAD / A / B / X / Y / LB / RB / Start / Select | `SDL_BUTTONS_MAPPING` 逐项映射到 `BUTTON_*` |
| 左右摇杆 | `SDL_CONTROLLER_AXIS_LEFTX/LEFTY/RIGHTX/RIGHTY`，死区 `16383.5f` |
| LT / RT 扳机 | `SDL_CONTROLLER_AXIS_TRIGGERLEFT/RIGHT`，死区 `3276.7f` |
| 震动 | `SDL_GameControllerRumble` |
| 有线手柄 | 清单声明 `android.hardware.usb.host` |
| 蓝牙手柄 | 清单声明 `android.hardware.bluetooth` |

清单里同时声明了 `android.hardware.gamepad`（`required="false"`），
所以带手柄的设备会被正确识别，手机也不会被过滤掉。

## 遥控器支持

Android TV 遥控器有两种上报方式，**两条路径都已打通**：

1. **遥控器声明了 `SOURCE_DPAD`**（绝大多数 Android TV 遥控器）
   → SDL 把它当摇杆处理，走 `SDLControllerManager.getButtonMask()`：

   | Android 键 | SDL 手柄键 | borealis |
   | --- | --- | --- |
   | `KEYCODE_DPAD_UP/DOWN/LEFT/RIGHT` | `DPAD_*` | `BUTTON_UP/DOWN/LEFT/RIGHT` + `BUTTON_NAV_*` |
   | `KEYCODE_DPAD_CENTER` | `A` | `BUTTON_A` |
   | `KEYCODE_BACK` | `B` | `BUTTON_B` |
   | `KEYCODE_MENU` | `START` | `BUTTON_X` |

2. **遥控器只有 `SOURCE_KEYBOARD`**
   → 走键盘路径，borealis 里有一段带注释的
   `// Android tv remote control` 代码把 `SDL_SCANCODE_UP/DOWN/LEFT/RIGHT`
   并入 `BUTTON_NAV_*`，`MENU → BUTTON_X`，`AC_BACK → BUTTON_B`。
   这条路径下 `KEYCODE_DPAD_CENTER` 会被 SDL 翻译成 `SDL_SCANCODE_SELECT`，
   而 borealis 不消费这个 scancode，所以 `WiliwiliActivity` 在
   `dispatchKeyEvent` 里把它改写成 `KEYCODE_ENTER`
   （→ `SDL_SCANCODE_RETURN` → `BUTTON_A`）。

**媒体键**在两条路径里都没有被覆盖：SDL 把 `KEYCODE_MEDIA_*` 映射成
`SDL_SCANCODE_AUDIO*`，borealis 不消费。`WiliwiliActivity` 把它们改写成
wiliwili 的默认快捷键（见 `config_helper.cpp`）：

| 遥控器按键 | 改写为 | wiliwili 默认快捷键 |
| --- | --- | --- |
| 播放 / 暂停 | `KEYCODE_SPACE` | `shortcut_video_pause` = `space` |
| 快进 | `KEYCODE_RIGHT_BRACKET` | `shortcut_forward` = `]` |
| 快退 | `KEYCODE_LEFT_BRACKET` | `shortcut_rewind` = `[` |

> 改写只发生在 `dispatchKeyEvent`，且 DPAD_CENTER 那一条会先用
> `SDLControllerManager.isDeviceSDLJoystick()` 判断，避免影响手柄本身
> 走摇杆路径的按键。

## 在 Android TV 上使用

APK 声明了 `LEANBACK_LAUNCHER`，并带一张 320×180 的 `tv_banner`
（`res/drawable-xhdpi/tv_banner.png`），因此会出现在 Android TV 的首页。
`android.software.leanback` 是 `required="false"`，手机同样可以安装。

## 构建

CI 走 `.github/workflows/android.yml`（GitHub Actions，`ubuntu-24.04`）。
本地构建需要 Android SDK + **NDK r26.1.10909125** + CMake 3.22.1：

```bash
# 1. host 侧工具：把 resources/ 打包进 .so
cd library/borealis && bash build_libromfs_generator.sh
cp libromfs-generator ../../libromfs-generator && cd ../..

# 2. 预编译 libmpv + mpv 头文件
bash .ci/prepare_mpv.sh

# 3. 签名用的 keystore（口令是公开的，仅用于让 APK 可安装）
keytool -genkeypair -v \
  -keystore android-project/app/wiliwili.jks -storetype JKS \
  -storepass wiliwili -keypass wiliwili \
  -alias wiliwili -keyalg RSA -keysize 2048 -validity 10000 \
  -dname "CN=wiliwili-android, OU=wiliwili, O=wiliwili, L=NA, S=NA, C=CN"

# 4. 构建
cd android-project && ./gradlew assembleRelease
```

### 为什么必须是 NDK r26

SDL 2.28 调用 `ALooper_pollAll()`，该符号在 NDK r27 中被移除。
`build.gradle` 里 `ndkVersion "26.1.10909125"` 就是这个原因。

### 16 KB 页对齐

`jni/CMakeLists.txt` 里加了 `-Wl,-z,max-page-size=16384`，
以适配 Android 15+ 的 16 KB 内存页内核。

### 为什么 libc++_shared.so 必须用 AAR 里那份（不能省）

`.ci/prepare_mpv.sh` 会把 libmpv AAR 里的 `libc++_shared.so` 一并取出，
再由 `.ci/align_libcxx.sh` 覆盖掉 NDK r26 sysroot 里的同名文件
（AGP 是从那里取 STL 打进 APK 的）。**两个独立原因，缺一不可：**

**1. 符号完整性（更严重）**

预编译的 `libmpv.so` 是从更新的 NDK 编出来的，它的未定义符号里有：

```
std::__ndk1::__from_chars_floating_point<double>(...)
std::__ndk1::__from_chars_floating_point<float>(...)
```

这是 libc++ 18 才导出的浮点 `from_chars` 实现。**NDK r26 的 libc++ 不导出它**，
所以 APK 里若用 NDK 那份，`libmpv.so` 在加载/首次调用时无法解析这两个符号。

用符号表实测（`_research/check_libcxx_swap.py`）：

| 库 | 对 NDK r26 libc++ 缺失 | 对 AAR libc++ 缺失 |
|---|---|---|
| `libmpv.so` | **2** | **0** |
| `libwiliwili.so`（本项目编译） | 0 | 0 |
| `libSDL2.so`（本项目编译） | 0 | 0 |

`armeabi-v7a` / `arm64-v8a` / `x86_64` 三个 ABI 结论一致。
（`__cxa_atexit` / `__cxa_finalize` 由 bionic libc 提供，两份 libc++ 都不导出，不算缺口。）

**2. 16 KB 页对齐**

| 库 | AAR 版对齐 | NDK r26 版对齐 |
|---|---|---|
| `libc++_shared.so`（arm64-v8a） | `0x4000` | `0x1000` |
| `libc++_shared.so`（x86_64） | `0x4000` | `0x1000` |

CI 里 `Verify APK` 会强制检查：APK 里的 `libc++_shared.so` 必须与 AAR 那份
**字节一致**，且所有 64 位 `.so` 的 LOAD 段 `p_align` 必须都是 `0x4000`，
另外还会用 `nm` 确认 `__from_chars_floating_point` 确实被导出。

### 为什么用 mbedtls 而不是 OpenSSL

NDK 不自带 OpenSSL。`jni/CMakeLists.txt` 用 `FetchContent` 把
mbedtls 3.5.2 编成静态库，并预置 `MBEDTLS_INCLUDE_DIRS` /
`MBEDTLS_LIBRARY` / `MBEDX509_LIBRARY` / `MBEDCRYPTO_LIBRARY`
四个变量（cpr 和 curl 的 `FindMbedTLS.cmake` 用的就是这四个名字），
`build.gradle` 再用 `-DCPR_FORCE_MBEDTLS_BACKEND=ON` 跳过 cpr 的
SSL 后端自动探测，避免不同 runner 上探测结果不一致。

## 第三方组件

见仓库根目录 `CREDITS.md`。
