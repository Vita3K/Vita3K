#!/bin/sh

export LDAI_VERBOSE=1

# Try to detect the target architecture from environment or fallback to host
if [ -n "$ARCH" ]; then
	DETECTED_ARCH="$ARCH"
elif [ -n "$CMAKE_APPIMAGE_ARCH" ]; then
	DETECTED_ARCH="$CMAKE_APPIMAGE_ARCH"
elif [ -n "$APPIMAGE_ARCH" ]; then
	DETECTED_ARCH="$APPIMAGE_ARCH"
else
	# Fallback to host architecture
	UNAME_ARCH=$(uname -m)
	case "$UNAME_ARCH" in
		x86_64)
			DETECTED_ARCH="x86_64" ;;
		aarch64|arm64)
			DETECTED_ARCH="aarch64" ;;
		*)
			DETECTED_ARCH="$UNAME_ARCH" ;;
	esac
fi

echo "Detected architecture: $DETECTED_ARCH"
export LDAI_UPDATE_INFORMATION="gh-releases-zsync|Vita3K|Vita3K|continuous|Vita3K-$(echo "$DETECTED_ARCH").AppImage.zsync"

$@
