// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceSblLicMgr.h"

EXPORT(int, sceSblLicMgrActivateDevkit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblLicMgrActivateFromFs) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblLicMgrClearActivationData) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblLicMgrGetActivationKey) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblLicMgrGetExpireDate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblLicMgrGetIssueNo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblLicMgrGetLicenseStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblLicMgrGetUsageTimeLimit) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceSblLicMgrActivateDevkit)
BRIDGE_IMPL(sceSblLicMgrActivateFromFs)
BRIDGE_IMPL(sceSblLicMgrClearActivationData)
BRIDGE_IMPL(sceSblLicMgrGetActivationKey)
BRIDGE_IMPL(sceSblLicMgrGetExpireDate)
BRIDGE_IMPL(sceSblLicMgrGetIssueNo)
BRIDGE_IMPL(sceSblLicMgrGetLicenseStatus)
BRIDGE_IMPL(sceSblLicMgrGetUsageTimeLimit)
