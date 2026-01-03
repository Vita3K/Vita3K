#!/bin/sh

# Note: The documentation at https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/blob/master/README.md for embedding update info is wrong.
# Detect architecture
# The correct env-variable name to use is UPDATE_INFORMATION, and *not* LDAI_UPDATE_INFORMATION.

# Detect architecture
ARCH=$(uname -m)

if [ "$ARCH" = "x86_64" ]; then
    APPIMAGE_NAME="Vita3K-x86_64.AppImage"
elif [ "$ARCH" = "aarch64" ]; then
    APPIMAGE_NAME="Vita3K-aarch64.AppImage"
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

echo "Executing linuxdeploy with cmdline: LDAI_VERBOSE=1 UPDATE_INFORMATION=\"gh-releases-zsync|Vita3K|Vita3K|latest|${APPIMAGE_NAME}.zsync\" $@"

LDAI_VERBOSE=1 UPDATE_INFORMATION="gh-releases-zsync|Vita3K|Vita3K|latest|${APPIMAGE_NAME}.zsync" $@
