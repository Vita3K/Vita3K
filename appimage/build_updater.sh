#!/bin/sh

# Note: The documentation at https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/blob/master/README.md for embedding update info is wrong.
# The correct env-variable name to use is UPDATE_INFORMATION, and *not* LDAI_UPDATE_INFORMATION.


ARCH=$(uname -m)

#detec arch
if [ "$ARCH" = "x86_64" ]; then
    UPDATE_INFO="gh-releases-zsync|Vita3K|Vita3K|latest|Vita3K-x86_64.AppImage.zsync"
elif [ "$ARCH" = "aarch64" ]; then
    UPDATE_INFO="gh-releases-zsync|Vita3K|Vita3K|latest|Vita3K-aarch64.AppImage.zsync"
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

LDAI_VERBOSE=1 UPDATE_INFORMATION="$UPDATE_INFO" $@
