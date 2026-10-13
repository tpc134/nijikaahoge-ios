#!/bin/bash
# Compile and link the delivered sources against the user's installed Apple SDK.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
VERSION="$(xcrun --sdk iphoneos --show-sdk-version)"
if [[ "$VERSION" != "9.3" ]]; then
    printf 'Expected iOS 9.3 SDK, found %s. Select Xcode 7.3 or 7.3.1 with xcode-select.\n' "$VERSION" >&2
    exit 1
fi
OUT="$ROOT/Tests/out/apple-sdk"
mkdir -p "$OUT"
MACSDK="$(xcrun --sdk macosx --show-sdk-path)"
xcrun --sdk macosx clang++ -isysroot "$MACSDK" -std=c++11 -stdlib=libc++ \
    -fobjc-arc -fexceptions -I"$ROOT/Sources" \
    "$ROOT/Sources/AHImageLoader.mm" "$ROOT/Tests/AppleImageTests.mm" \
    -framework Foundation -framework CoreGraphics -framework ImageIO -o "$OUT/AppleImageTests"
"$OUT/AppleImageTests" "$ROOT/Resources/AhogeAssets"
for ARCH in armv7 arm64; do
    MINIMUM=6.0
    if [[ "$ARCH" == "arm64" ]]; then MINIMUM=7.0; fi
    DIR="$OUT/$ARCH"
    mkdir -p "$DIR"
    OBJECTS=()
    while IFS= read -r SOURCE; do
        NAME="${SOURCE#"$ROOT/"}"
        NAME="${NAME//\//_}"
        OBJECT="$DIR/$NAME.o"
        FLAGS=()
        if [[ "$SOURCE" == *.mm ]]; then FLAGS=(-fobjc-arc); fi
        xcrun --sdk iphoneos clang++ -arch "$ARCH" -isysroot "$SDK" \
            -miphoneos-version-min="$MINIMUM" -std=c++11 -stdlib=libc++ -O2 \
            -fexceptions -I"$ROOT/Sources" -I"$ROOT/Vendor" \
            "${FLAGS[@]}" -c "$SOURCE" -o "$OBJECT"
        OBJECTS+=("$OBJECT")
    done < <(find "$ROOT/Sources" "$ROOT/Vendor/Box2D" -type f \( -name '*.cpp' -o -name '*.mm' \) | sort)
    xcrun --sdk iphoneos clang++ -arch "$ARCH" -isysroot "$SDK" \
        -miphoneos-version-min="$MINIMUM" -stdlib=libc++ -dynamiclib \
        "${OBJECTS[@]}" -framework UIKit -framework Foundation \
        -framework CoreGraphics -framework QuartzCore -framework GLKit \
        -framework OpenGLES -framework AudioToolbox -framework AVFoundation -framework ImageIO \
        -o "$DIR/Ahoge-link-check.dylib"
    printf 'Apple SDK %s: %s (minimum %s) compile and link passed.\n' "$VERSION" "$ARCH" "$MINIMUM"
done
printf 'This checks sources, not signing, app packaging, or device behavior.\n'
