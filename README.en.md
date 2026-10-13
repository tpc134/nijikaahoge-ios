# Nijika's Ahoge — iOS

[简体中文](README.md) | **English** | [日本語](README.ja.md)

![Nijika's Ahoge running on a real iPhone, iPad and Android device](readme_pic/1.jpg)

**An unofficial iOS port of *Nijika's Ahoge*.**

| | |
| --- | --- |
| Original work | **Nijika's Ahoge** |
| Original link | <https://tamanidamani.itch.io/nijikas-ahoge> |
| Original authors | **ACh (ナコ)** and **TamaniDamani (塔打)** |
| Permission | **Granted by both original authors**, on the conditions of **attribution** and **non-commercial use**. This port is **free of charge and free of advertising**. |

Targets old devices running **iOS 6.0 and later**. The stack is
**Objective-C++ / C++11 + OpenGL ES 2 + Box2D 2.2.1 + Core Audio RemoteIO**.
No CocoaPods, and no runtime dependency is downloaded over the network.

Game logic was ported module by module from the project's own Android v1.2 sources.

| | |
| --- | --- |
| Target OS | iOS 6.0 and later |
| Base SDK | iOS 9.3 (Xcode 7.3 / 7.3.1) |
| Architecture | armv7 (optional arm64 slice for iOS 7+) |
| Bundle ID | `com.ruich97.nijikaahoge` |
| Language | Objective-C++ / C++11, libc++, ARC, C++ exceptions on, Bitcode off |
| Graphics / physics / audio | OpenGL ES 2 / Box2D 2.2.1 / Core Audio RemoteIO |
| Project | `Nijika's Ahoge.xcodeproj`, shared scheme `Nijika's Ahoge iOS6` |

## Download

A prebuilt IPA is published on the **Releases** page. It is pseudo-signed with **ldid**, so it installs
on:

- **jailbroken devices**, or
- **TrollStore** (supported system versions are governed by TrollStore's own documentation)

This is not an App Store signature, nor an ordinary personal / enterprise certificate signature.
Devices that are neither jailbroken nor running TrollStore cannot install it directly. Builds you
compile yourself also need signing.

## Build status

**Compiled and linked in Xcode, running on a real iPhone and iPad, with an ldid-signed IPA produced
(see the banner image above).**

Verified so far:

- **Apple side**: opens, builds and links in Xcode 7.3.1 against the iOS 9.3 SDK; the product runs on
  an iPhone and an iPad (banner image).
- **Signing**: the IPA is pseudo-signed with **ldid**, targeting jailbroken / TrollStore installation.
- **Windows side**: 25 structural integration checks (original project format and its three targets,
  unchanged signing and test settings, reference closure, portable relative paths, unique build
  membership, SDK / deployment target / architecture settings, programmatic entry point, orientation and
  status bar keys, icons, shared scheme); all 33 portable core tests pass, including every `CoreTests`
  case; exactly 55 application sources (5 game C++, 3 Objective-C++, 45 Box2D C++, plus the original
  `AppDelegate.m` and `main.m`, with no test `main` linked); 9 linked system frameworks, none embedded;
  9 bundle entries.

Not yet accepted item by item: `actool` output and ImageIO pixel-test details, behaviour on legacy
devices (iPhone 3GS/4/4S/5, iPad 2/3 and similar), background and audio-interruption handling, and
low-end performance benchmarks.

## Building on a Mac

**Xcode 7.3 or 7.3.1** (which ships the iOS 9.3 SDK) on OS X 10.11 or later.

1. Open `Nijika's Ahoge.xcodeproj` and select the shared scheme **Nijika's Ahoge iOS6**. No separate
   project or app target was created; the original project ID, Bundle ID and signing settings are kept.
2. In the app target's General tab, select your own Team / Provisioning Profile. The certificate must
   cover the current Bundle ID and the target device.
3. Build in Debug once to check launch. Use **Release** for low-end performance testing and the final
   Archive — do not judge shipping performance from Debug `-O0` frame rates.
4. To change the Bundle ID, edit it in the app target's Build Settings and match the certificate and
   provisioning profile accordingly.

Optional command-line build:

```sh
xcodebuild -project "Nijika's Ahoge.xcodeproj" \
  -scheme "Nijika's Ahoge iOS6" -configuration Release \
  -sdk iphoneos9.3 -destination 'generic/platform=iOS' build
```

Then run the bundle checks against the real `.app` and complete the device checklist:

```sh
python3 AhogePort/Tools/check_app_bundle.py "/absolute/path/to/Nijika's Ahoge.app"
bash AhogePort/Tools/check_apple_sdk.sh
```

## Layout

| Path | Contents |
| --- | --- |
| `Nijika's Ahoge/` | App target: `AppDelegate`, `main.m`, `Info.plist`, icons, legacy launch images |
| `AhogePort/Sources/` | Port sources (5 game C++ + 3 Objective-C++) |
| `AhogePort/Vendor/Box2D/` | Box2D 2.2.1 upstream sources (45 `.cpp`) |
| `AhogePort/Resources/AhogeAssets/` | 75 animation frames + 12 WAV clips |
| `AhogePort/Resources/LegacyLaunch/` | 7 legacy `Default` launch images |
| `AhogePort/Tests/` | Portable core tests, Apple image tests, bundle checks |
| `AhogePort/Tools/` | Asset preparation / verification / packaging scripts |
| `AhogePort/Verification/` | Verification results and screenshots |
| `readme_pic/` | Banner image used at the top of the READMEs |

### Source modules

| File | Role |
| --- | --- |
| `AHCore.hpp` | Core data structures: `Data` / `Obj` / `Game` / `Tween` / `Scheduler` / `Emitter` / `Viewport` |
| `AHData.generated.cpp` | Data tables generated from `game_data.json` |
| `AHGeometry.cpp` | Polygon cleanup, triangulation, convexity test |
| `AHGame.cpp` | Game logic, mirroring Android's `Logic.kt` / `World.kt` / `Phys.kt` |
| `AHSystems.cpp` | Tweens and sequence scheduling, mirroring `Sys.kt` |
| `AHRenderer.cpp` | OpenGL ES 2 renderer |
| `AHImageLoader.mm` | ImageIO / CoreGraphics decode into pixel buffers |
| `AHAudio.mm`, `AHAudioMixer.hpp`, `AHAudioState.hpp`, `AHWave.hpp` | Core Audio RemoteIO output, 12-voice mixer (dB + linear L/R pan), WAV parsing |
| `AHGameViewController.mm` | GL view, touch handling, orientation, vibration, lifecycle |

## Differences from the Android build

| | Android v1.2 | iOS |
| --- | --- | --- |
| Physics | jBox2D 2.2.1.1 | Box2D 2.2.1 (upstream C++) |
| Sound format | AAC / M4A | WAV (PCM) |
| Haptics | 9 ms / 55 ms adjustable | System-standard vibration |
| Build | Gradle / Kotlin | Xcode / Objective-C++ |
| Minimum OS | Android 5.0 | iOS 6.0 |

> "Every branch has been ported" is not the same as "every iOS device check has passed." Numeric
> trajectories do not match bit-for-bit between Java and C++ or across different time steps either.

## License, permission and credits

### Original work

| | |
| --- | --- |
| Title | **Nijika's Ahoge** |
| Original link | <https://tamanidamani.itch.io/nijikas-ahoge> |
| Authors | **ACh (ナコ)** and **TamaniDamani (塔打)** |

**Permission: granted by both original authors**, on the conditions of attribution and
non-commercial use. This port is free of charge, carries no advertising, and ships no paid content
or ad SDK.

### Asset copyright (important)

All game artwork, audio, characters and other assets originating from the original work remain the
property of their **original authors (the artist)** and are **NOT covered by this repository's MIT
license**. This repository claims no copyright over them and grants no right to use them. The original
author's signature remains visible in the game visuals. Do not use the original authors' likeness or
official promotional art as a cover image — use your own gameplay screenshots instead.

### Code license

The source code written for this port is released under the **MIT License** — see [`LICENSE`](LICENSE).
That license applies **only to the code written for this project** and does not extend to any of the
original assets described above.

### Third-party components

- Box2D **2.2.1** comes from the original project's official Google Code archive and includes only the
  engine's 45 `.cpp` files and related headers. The sources are unmodified and retain their in-file
  license plus [`AhogePort/Vendor/Box2D-LICENSE.txt`](AhogePort/Vendor/Box2D-LICENSE.txt).
- The Apple SDK, system frameworks and signing material are not distributed with this repository.
- This repository is an **unofficial fan port**, unaffiliated with and not endorsed by the original authors.
