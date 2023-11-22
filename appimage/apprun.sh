#!/bin/sh
if [ "${APPIMAGE}" != "" ]; then
	"${APPDIR}/usr/bin/Vita3K" $@
fi
