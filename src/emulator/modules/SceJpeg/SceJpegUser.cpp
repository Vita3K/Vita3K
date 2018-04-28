// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "SceJpegUser.h"

EXPORT(int, sceJpegCreateSplitDecoder) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegCsc) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegDecodeMJpeg) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegDecodeMJpegYCbCr) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegDeleteSplitDecoder) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegFinishMJpeg) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegGetOutputInfo) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegInitMJpeg) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegInitMJpegWithParam) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegMJpegCsc) {
    return unimplemented(export_name);
}

EXPORT(int, sceJpegSplitDecodeMJpeg) {
    return unimplemented(export_name);
}

BRIDGE_IMPL(sceJpegCreateSplitDecoder)
BRIDGE_IMPL(sceJpegCsc)
BRIDGE_IMPL(sceJpegDecodeMJpeg)
BRIDGE_IMPL(sceJpegDecodeMJpegYCbCr)
BRIDGE_IMPL(sceJpegDeleteSplitDecoder)
BRIDGE_IMPL(sceJpegFinishMJpeg)
BRIDGE_IMPL(sceJpegGetOutputInfo)
BRIDGE_IMPL(sceJpegInitMJpeg)
BRIDGE_IMPL(sceJpegInitMJpegWithParam)
BRIDGE_IMPL(sceJpegMJpegCsc)
BRIDGE_IMPL(sceJpegSplitDecodeMJpeg)
