#!/bin/sh
if [ "${APPIMAGE}" != "" ]; then
	XDG_DATA_DIRS="${APPDIR}/usr/share:${XDG_DATA_DIRS}" "${APPDIR}/usr/bin/Vita3K" $@
fi
