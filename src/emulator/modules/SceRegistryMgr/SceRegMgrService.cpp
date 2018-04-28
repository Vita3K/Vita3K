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

#include "SceRegMgrService.h"

EXPORT(int, sceRegMgrSrvCnvRegionInt) {
    return unimplemented(export_name);
}

EXPORT(int, sceRegMgrSrvCnvRegionPsCode) {
    return unimplemented(export_name);
}

EXPORT(int, sceRegMgrSrvCnvRegionStr) {
    return unimplemented(export_name);
}

EXPORT(int, sceRegMgrSrvGetRegion) {
    return unimplemented(export_name);
}

EXPORT(int, sceRegMgrSrvGetRegionStr) {
    return unimplemented(export_name);
}

BRIDGE_IMPL(sceRegMgrSrvCnvRegionInt)
BRIDGE_IMPL(sceRegMgrSrvCnvRegionPsCode)
BRIDGE_IMPL(sceRegMgrSrvCnvRegionStr)
BRIDGE_IMPL(sceRegMgrSrvGetRegion)
BRIDGE_IMPL(sceRegMgrSrvGetRegionStr)
