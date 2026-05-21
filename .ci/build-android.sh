#!/usr/bin/env bash
set -euo pipefail

OUTPUT_DIR="${1:-}"
if [[ -z "$OUTPUT_DIR" ]]; then
    echo "Usage: $0 <output-dir>" >&2
    exit 1
fi

mkdir -p android/app/assets
rm -rf android/app/assets/data android/app/assets/shaders-builtin
cp -r data android/app/assets/data
cp -r vita3k/shaders-builtin android/app/assets/shaders-builtin

export JAVA_HOME="${JAVA_HOME_17_X64}"
chmod +x android/gradlew

if [[ -e "${SIGNING_STORE_PATH:-}" ]]; then
    BUILD_TYPE="Release"
else
    BUILD_TYPE="Reldebug"
fi

pushd android > /dev/null
./gradlew --stacktrace --configuration-cache --build-cache --parallel --configure-on-demand ":app:assemble${BUILD_TYPE}"
popd > /dev/null

APK_PATH="android/app/build/outputs/apk/${BUILD_TYPE,,}/app-${BUILD_TYPE,,}.apk"
mkdir -p "$OUTPUT_DIR"
cp "$APK_PATH" "$OUTPUT_DIR/app.apk"
