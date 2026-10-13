# Nijika's Ahoge — iOS 版

[简体中文](README.md) | [English](README.en.md) | **日本語**

![iPhone・iPad・Android 実機での Nijika's Ahoge 実行画面](readme_pic/1.jpg)

**『Nijika's Ahoge』（虹夏ちゃんの無限ドリトス）の非公式 iOS 移植版です。**

| 項目 | 内容 |
| --- | --- |
| 原作 | **Nijika's Ahoge**（虹夏ちゃんの無限ドリトス） |
| 原作リンク | <https://tamanidamani.itch.io/nijikas-ahoge> |
| 原作者 | **ACh（ナコ）**、**TamaniDamani（塔打）** |
| 許諾 | **原作者お二方の同意を得ています**。条件は**氏名表示**と**非営利**。本移植は**無料・広告なし**です。 |

**iOS 6.0 以降**の旧端末を対象にしています。技術スタックは
**Objective-C++ / C++11 + OpenGL ES 2 + Box2D 2.2.1 + Core Audio RemoteIO**。
CocoaPods は使わず、実行時にネットワークから依存を取得することもありません。

ゲームロジックはプロジェクト自身が提供した Android v1.2 のソースからモジュール単位で移植しています。

| 項目 | 値 |
| --- | --- |
| 対象 OS | iOS 6.0 以降 |
| Base SDK | iOS 9.3（Xcode 7.3 / 7.3.1） |
| アーキテクチャ | armv7（iOS 7+ 向けに arm64 スライスも可） |
| Bundle ID | `com.ruich97.nijikaahoge` |
| 言語 | Objective-C++ / C++11、libc++、ARC、C++ 例外オン、Bitcode オフ |
| グラフィックス / 物理 / オーディオ | OpenGL ES 2 / Box2D 2.2.1 / Core Audio RemoteIO |
| プロジェクト | `Nijika's Ahoge.xcodeproj`、共有スキーム `Nijika's Ahoge iOS6` |

## ダウンロード

ビルド済み IPA は **Releases** ページで公開します。**ldid** による仮署名のため、インストール先は：

- **脱獄（ジェイルブレイク）済み端末**、または
- **TrollStore**（対応するシステムバージョンは TrollStore 公式の説明に従います）

App Store 署名でも、通常の個人 / 法人証明書による署名でもありません。脱獄も TrollStore も
導入していない端末には直接インストールできません。ご自身でビルドした場合も署名が別途必要です。

## ビルド状況

**Xcode でのコンパイルとリンクに成功し、iPhone と iPad の実機で動作、ldid 署名済み IPA も生成済みです
（冒頭の画像参照）。**

検証済みの内容：

- **Apple 側**：Xcode 7.3.1 + iOS 9.3 SDK での open / build / link が成功。成果物は iPhone と iPad で
  実際に動作（冒頭の画像）。
- **署名**：IPA は **ldid** による仮署名で、脱獄 / TrollStore 向けです。
- **Windows 側**：25 項目のプロジェクト構造チェック（元プロジェクトの形式と 3 つの target、署名と
  テスト設定が未変更であること、参照の閉包性、可搬な相対パス、ビルドメンバーの一意性、
  SDK / Deployment Target / アーキテクチャ設定、プログラム起動への変更、向きとステータスバーのキー、
  アイコン、共有スキーム）、33 項目の可搬コアテストがすべて通過（`CoreTests` の全ケースを含む）、
  アプリケーションソースはちょうど 55 個（ゲーム C++ が 5、Objective-C++ が 3、Box2D C++ が 45、
  加えて元の `AppDelegate.m` と `main.m`、テスト用 `main` はリンクせず）、リンクするシステム
  フレームワークは 9 個（埋め込みなし）、Bundle エントリは 9 項目。

まだ項目単位で受け入れていないもの：`actool` の出力と ImageIO 画素テストの詳細、旧機種
（iPhone 3GS/4/4S/5、iPad 2/3 など）での個別の挙動、バックグラウンドとオーディオ割り込みの処理、
低スペック端末での性能ベンチマーク。

## Mac でのビルド

**Xcode 7.3 または 7.3.1**（iOS 9.3 SDK 同梱）、OS X 10.11 以降が必要です。

1. `Nijika's Ahoge.xcodeproj` を開き、共有スキーム **Nijika's Ahoge iOS6** を選択します。
   別のプロジェクトやアプリ target は作成しておらず、元のプロジェクト ID、Bundle ID、
   署名設定をそのまま保持しています。
2. アプリ target の General で自分の Team / Provisioning Profile を選びます。
   証明書は現在の Bundle ID と対象端末を含む必要があります。
3. まず Debug で起動確認。旧端末の性能測定と最終 Archive は **Release** で行ってください。
   Debug の `-O0` のフレームレートで製品性能を判断しないでください。
4. Bundle ID を変更する場合はアプリ target の Build Settings で変更し、
   証明書とプロビジョニングプロファイルも合わせてください。

コマンドラインでのビルド（任意）：

```sh
xcodebuild -project "Nijika's Ahoge.xcodeproj" \
  -scheme "Nijika's Ahoge iOS6" -configuration Release \
  -sdk iphoneos9.3 -destination 'generic/platform=iOS' build
```

そのあと実際の `.app` に対してバンドル検証を実行し、チェックリストに沿って実機で確認します。

```sh
python3 AhogePort/Tools/check_app_bundle.py "/absolute/path/to/Nijika's Ahoge.app"
bash AhogePort/Tools/check_apple_sdk.sh
```

## ディレクトリ構成

| パス | 内容 |
| --- | --- |
| `Nijika's Ahoge/` | アプリ target：`AppDelegate`、`main.m`、`Info.plist`、アイコン、旧式起動画像 |
| `AhogePort/Sources/` | 移植ソース（ゲーム C++ 5 個 + Objective-C++ 3 個） |
| `AhogePort/Vendor/Box2D/` | Box2D 2.2.1 公式ソース（`.cpp` 45 個） |
| `AhogePort/Resources/AhogeAssets/` | アニメーションフレーム 75 枚 + WAV 12 本 |
| `AhogePort/Resources/LegacyLaunch/` | 旧式 `Default` 起動画像 7 枚 |
| `AhogePort/Tests/` | 可搬コアテスト、Apple 画像テスト、バンドル検査 |
| `AhogePort/Tools/` | アセット準備 / 検証 / パッケージングのスクリプト |
| `AhogePort/Verification/` | 検証結果とスクリーンショット |
| `readme_pic/` | README 冒頭のバナー画像 |

### ソースモジュール

| ファイル | 役割 |
| --- | --- |
| `AHCore.hpp` | コアデータ構造：`Data` / `Obj` / `Game` / `Tween` / `Scheduler` / `Emitter` / `Viewport` |
| `AHData.generated.cpp` | `game_data.json` から生成したデータテーブル |
| `AHGeometry.cpp` | 多角形のクリーニング、三角形分割、凸判定 |
| `AHGame.cpp` | ゲームロジック。Android の `Logic.kt` / `World.kt` / `Phys.kt` に対応 |
| `AHSystems.cpp` | Tween とシーケンス制御。`Sys.kt` に対応 |
| `AHRenderer.cpp` | OpenGL ES 2 レンダラ |
| `AHImageLoader.mm` | ImageIO / CoreGraphics で画素バッファへデコード |
| `AHAudio.mm`、`AHAudioMixer.hpp`、`AHAudioState.hpp`、`AHWave.hpp` | Core Audio RemoteIO 出力、12 声部ミキサ（dB + 線形 L/R pan）、WAV 解析 |
| `AHGameViewController.mm` | GL ビュー、タッチ、向き、振動、ライフサイクル |

## Android 版との違い

| 項目 | Android v1.2 | iOS |
| --- | --- | --- |
| 物理エンジン | jBox2D 2.2.1.1 | Box2D 2.2.1（公式 C++） |
| 効果音の形式 | AAC / M4A | WAV（PCM） |
| 振動 | 9ms / 55ms の可変長 | システム標準の振動 |
| ビルド | Gradle / Kotlin | Xcode / Objective-C++ |
| 最低 OS | Android 5.0 | iOS 6.0 |

> 「すべての分岐を移植済み」は「iOS 実機の全項目を受け入れ済み」と同じではありません。
> Java と C++、あるいは異なるタイムステップ間で数値の軌跡がビット単位で一致することも保証しません。

## ライセンス・許諾・クレジット

### 原作

| 項目 | 内容 |
| --- | --- |
| 原作名 | **Nijika's Ahoge**（虹夏ちゃんの無限ドリトス） |
| 原作リンク | <https://tamanidamani.itch.io/nijikas-ahoge> |
| 原作者 | **ACh（ナコ）**、**TamaniDamani（塔打）** |

**許諾状況：原作者お二方の同意を得ています。条件は氏名表示と非営利であり、
本移植は無料・広告なしで、有料コンテンツも広告 SDK も含みません。**

### 素材の著作権（重要）

原作に由来するゲームの画像、音声、キャラクターその他の素材の著作権は
**原作者（イラストレーター）に帰属し**、**本リポジトリの MIT ライセンスの対象外です**。
本リポジトリはこれらの素材について著作権を主張せず、MIT ライセンスによって
利用権を許諾することもありません。原作画面の原作者署名は表示されたままにしています。
原作者の肖像や公式のプロモーション画像をカバーに使わず、ご自身で撮影したプレイ画面を使用してください。

### コードのライセンス

本移植が独自に記述したソースコードは **MIT ライセンス**で公開します（[`LICENSE`](LICENSE)）。
その許諾範囲は**本プロジェクトが独自に記述したコードに限られ**、上記の原作素材は含みません。

### サードパーティ

- Box2D **2.2.1** は元プロジェクトの公式 Google Code アーカイブから取得したもので、
  エンジンの `.cpp` 45 個と関連ヘッダのみを含みます。ソースは未改変で、
  ファイル内のライセンスと [`AhogePort/Vendor/Box2D-LICENSE.txt`](AhogePort/Vendor/Box2D-LICENSE.txt)
  を保持しています。
- Apple SDK、システムフレームワーク、署名素材は本リポジトリに含まれません。
- 本リポジトリは**非公式の二次創作移植**であり、原作者とは無関係で、原作者の見解を示すものでもありません。
