#!/bin/sh
if [ "${APPIMAGE}" != "" ]; then
	export PATH="$APPDIR/usr/bin:$PATH"
	"${APPDIR}/usr/bin/Vita3K" $@
fi
