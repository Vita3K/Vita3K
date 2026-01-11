#!/bin/sh

# Note: The documentation at https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/blob/master/README.md for embedding update info is wrong.
# Detect architecture
# The correct env-variable name to use is UPDATE_INFORMATION, and *not* LDAI_UPDATE_INFORMATION.

# Detect architecture
ARCH=$(uname -m)

case "$ARCH" in
    x86_64|amd64) APPIMAGE_NAME="Vita3K-x86_64.AppImage" ;;
    aarch64|arm64) APPIMAGE_NAME="Vita3K-aarch64.AppImage" ;;
    *)
      echo "Unsupported architecture: $ARCH"
      exit 1
      ;;
esac

echo "Executing linuxdeploy with cmdline: LDAI_VERBOSE=1 UPDATE_INFORMATION=\"gh-releases-zsync|Vita3K|Vita3K|latest|${APPIMAGE_NAME}.zsync\" $@"

LDAI_VERBOSE=1 UPDATE_INFORMATION="gh-releases-zsync|Vita3K|Vita3K|latest|${APPIMAGE_NAME}.zsync" $@
