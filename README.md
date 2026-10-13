# Nijika's Ahoge — iOS 版

**简体中文** | [English](README.en.md) | [日本語](README.ja.md)

![Nijika's Ahoge 在 iPhone、iPad 与 Android 真机上的实机画面](readme_pic/1.jpg)

**《虹夏的呆毛》的非官方 iOS 移植版。**

| | |
| --- | --- |
| 原作 | **Nijika's Ahoge** |
| 别名 | 虹夏呆毛无限增殖中 / 呆毛可以无限增殖的虹夏酱 / 虹夏ちゃんの無限ドリトス |
| 原作链接 | <https://tamanidamani.itch.io/nijikas-ahoge> |
| 原作作者 | **ACh（ナコ）**、**TamaniDamani（塔打）** |
| 授权 | **已获得两位原作者的同意**，条件为**署名**与**非营利**；本移植**免费、无广告**。 |

面向 **iOS 6.0 及以上**的旧设备。技术栈为 **Objective-C++ / C++11 + OpenGL ES 2 + Box2D 2.2.1 + Core Audio RemoteIO**，
不依赖 CocoaPods，运行时不联网下载任何依赖。

游戏逻辑从项目自身的 Android v1.2 源码逐模块移植。

| 项目 | 值 |
| --- | --- |
| 目标系统 | iOS 6.0 及以上 |
| Base SDK | iOS 9.3（Xcode 7.3 / 7.3.1） |
| 架构 | armv7 + arm64 切片（armv7 覆盖 iOS 6，arm64 覆盖 iOS 7 及以上） |
| Bundle ID | `com.ruich97.nijikaahoge` |
| 语言 | Objective-C++ / C++11，libc++，ARC，C++ 异常开启，Bitcode 关闭 |
| 图形 / 物理 / 音频 | OpenGL ES 2 / Box2D 2.2.1 / Core Audio RemoteIO |
| 工程 | `Nijika's Ahoge.xcodeproj`，共享 Scheme `Nijika's Ahoge iOS6` |

## 下载

预编译 IPA 发布在 **Releases** 页面，使用 **ldid** 伪签名，安装方式为：

- **越狱设备**，或
- **TrollStore**（可支持的系统版本以 TrollStore 官方说明为准）

这不是 App Store 签名，也不是普通个人 / 企业证书签名；未越狱且未安装 TrollStore 的设备无法直接安装。
自建产物同样需要自行签名。

已在 **iPhone 4S（iOS 6）** 与 **iPhone SE（iOS 15）** 上实测运行正常。

## 构建状态

**已在 Xcode 中编译、链接成功并产出 ldid 签名 IPA；实测在 iPhone 4S（iOS 6）与 iPhone SE（iOS 15）上运行正常（见题图）。**

已完成的验证：

- **Apple 侧**：Xcode 7.3.1 + iOS 9.3 SDK 打开、构建、链接通过；产物在 iPhone 4S（iOS 6）与
  iPhone SE（iOS 15）上实测运行正常（题图）。
- **签名**：IPA 为 **ldid** 伪签名，面向越狱 / TrollStore 安装。
- **Windows 侧**：25 项工程结构检查（原工程格式与三个 target、签名与测试设置未改动、引用闭合、
  相对路径可移植、构建成员唯一、SDK / 部署目标 / 架构设置、入口改为程序化、方向与状态栏键、图标、共享 Scheme）；
  33 项可移植核心测试全部通过（含 `CoreTests` 全部用例）；55 个应用源文件引用闭合
  （5 个游戏 C++、3 个 Objective-C++、45 个 Box2D C++，以及原工程 `AppDelegate.m` 与 `main.m`，
  未链接任何测试 `main`）；9 个链接系统框架（无嵌入）；9 项 Bundle 条目。

尚未逐项验收的边缘项目：`actool` 输出与 ImageIO 像素测试细节、iPhone 3GS / 4 / 5 与 iPad 2 / 3 等
其他旧机型、前后台切换与音频中断行为、长时间运行的发热与功耗。

## 在 Mac 上构建

需要 **Xcode 7.3 或 7.3.1**（自带 iOS 9.3 SDK），运行环境 OS X 10.11 或更新。

1. 用 Xcode 打开本目录的 `Nijika's Ahoge.xcodeproj`，选择共享 Scheme **Nijika's Ahoge iOS6**。
   没有另建工程或应用 target，原工程 ID、Bundle ID 与签名设置均保留。
2. 在应用 target 的 General 中选择你自己的 Team / Provisioning Profile；
   证书必须包含当前 Bundle ID 与目标设备。
3. 首次用 Debug 构建检查启动；旧机性能测试与最终 Archive 用 **Release**，
   不要用 Debug 的 `-O0` 帧率判断成品性能。
4. 如需修改 Bundle ID，在应用 target 的 Build Settings 中修改，证书与描述文件同步匹配。

命令行构建（可选）：

```sh
xcodebuild -project "Nijika's Ahoge.xcodeproj" \
  -scheme "Nijika's Ahoge iOS6" -configuration Release \
  -sdk iphoneos9.3 -destination 'generic/platform=iOS' build
```

构建完成后对实际 `.app` 执行打包校验，并按清单在真机上验收：

```sh
python3 AhogePort/Tools/check_app_bundle.py "/absolute/path/to/Nijika's Ahoge.app"
bash AhogePort/Tools/check_apple_sdk.sh
```

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `Nijika's Ahoge/` | 应用 target：`AppDelegate`、`main.m`、`Info.plist`、图标、传统启动图 |
| `AhogePort/Sources/` | 移植源码（5 个游戏 C++ + 3 个 Objective-C++） |
| `AhogePort/Vendor/Box2D/` | Box2D 2.2.1 官方源码（45 个 `.cpp`） |
| `AhogePort/Resources/AhogeAssets/` | 75 张动画帧 + 12 段 WAV |
| `AhogePort/Resources/LegacyLaunch/` | 7 张传统 `Default` 启动图 |
| `AhogePort/Tests/` | 可移植核心测试、Apple 图片测试、Bundle 检查 |
| `AhogePort/Tools/` | 资源准备 / 校验 / 打包脚本 |
| `AhogePort/Verification/` | 验证结果与截图 |
| `readme_pic/` | README 顶部题图 |

### 源码模块

| 文件 | 职责 |
| --- | --- |
| `AHCore.hpp` | 核心数据结构：`Data` / `Obj` / `Game` / `Tween` / `Scheduler` / `Emitter` / `Viewport` |
| `AHData.generated.cpp` | 由 `game_data.json` 生成的数据表 |
| `AHGeometry.cpp` | 多边形清理、三角化、凸性判定 |
| `AHGame.cpp` | 游戏逻辑，对应 Android 的 `Logic.kt` / `World.kt` / `Phys.kt` |
| `AHSystems.cpp` | Tween 与序列调度，对应 `Sys.kt` |
| `AHRenderer.cpp` | OpenGL ES 2 渲染器 |
| `AHImageLoader.mm` | ImageIO / CoreGraphics 解码 → 像素缓冲 |
| `AHAudio.mm`、`AHAudioMixer.hpp`、`AHAudioState.hpp`、`AHWave.hpp` | Core Audio RemoteIO 输出、12 声部混音（dB + 线性 L/R pan）、WAV 解析 |
| `AHGameViewController.mm` | GL 视图、触摸、方向、振动、生命周期 |

## 与 Android 版的差异

| 项 | Android v1.2 | iOS |
| --- | --- | --- |
| 物理引擎 | jBox2D 2.2.1.1 | Box2D 2.2.1（官方 C++） |
| 音效格式 | AAC / M4A | WAV（PCM） |
| 振动 | 9ms / 55ms 可调时长 | 系统标准振动 |
| 构建 | Gradle / Kotlin | Xcode / Objective-C++ |
| 最低系统 | Android 5.0 | iOS 6.0 |

> 「全部分支已移植」不等于「iOS 真机全部验收通过」。Java / C++ 与不同步长的数值轨迹
> 也不承诺逐位相同。

## 版权、授权与致谢

### 原作

| | |
| --- | --- |
| 原作名 | **Nijika's Ahoge**（虹夏的呆毛） |
| 别名 | 虹夏呆毛无限增殖中 / 呆毛可以无限增殖的虹夏酱 / 虹夏ちゃんの無限ドリトス |
| 原作链接 | <https://tamanidamani.itch.io/nijikas-ahoge> |
| 原作作者 | **ACh（ナコ）**、**TamaniDamani（塔打）** |

**授权状态：已获得两位原作者的同意。条件为署名与非营利；本移植免费、无广告，
不含任何付费内容或广告 SDK。**

### 素材版权（重要）

游戏图像、音效、角色以及其他源自原作的素材，版权**归原作者（画师）所有**，
**不适用本仓库的 MIT 协议**。本仓库不对这些素材主张任何权利，也不通过 MIT 协议授予任何使用权。
原作画面中的原作者署名保持可见。请勿使用原作者的肖像或官方宣传图作为封面，请使用自行截取的游玩截图。

### 代码协议

本移植自身编写的源代码以 **MIT 协议**发布，全文见 [`LICENSE`](LICENSE)。
该协议的授权范围**仅限于本项目自行编写的代码**，不覆盖上述任何原作素材。

### 第三方组件

- Box2D **2.2.1** 取自原项目的官方 Google Code 归档，仅包含引擎的 45 个 `.cpp` 与相关头文件，
  源码未修改，保留文件内许可与 [`AhogePort/Vendor/Box2D-LICENSE.txt`](AhogePort/Vendor/Box2D-LICENSE.txt)。
- Apple SDK、系统框架与签名材料不随本仓库分发。
- 本仓库是**非官方同人移植**，与原作作者无关，也不代表其立场。
