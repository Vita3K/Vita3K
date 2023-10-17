#!/bin/sh

# Note: The documentation at https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/blob/master/README.md for embedding update info is wrong.
# The correct env-variable name to use is UPDATE_INFORMATION, and *not* LDAI_UPDATE_INFORMATION.

echo "Executing linuxdeploy with cmdline: LDAI_VERBOSE=1 UPDATE_INFORMATION=\"gh-releases-zsync|Vita3K|Vita3K|latest|Vita3K-x86_64.AppImage.zsync\" $@"

LDAI_VERBOSE=1 UPDATE_INFORMATION="gh-releases-zsync|Vita3K|Vita3K|latest|Vita3K-x86_64.AppImage.zsync" $@
