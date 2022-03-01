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

#pragma once

#include <module/module.h>

BRIDGE_DECL(sceNpBandwidthTestAbort)
BRIDGE_DECL(sceNpBandwidthTestGetStatus)
BRIDGE_DECL(sceNpBandwidthTestInitStart)
BRIDGE_DECL(sceNpBandwidthTestShutdown)
BRIDGE_DECL(sceNpLookupAbortRequest)
BRIDGE_DECL(sceNpLookupAvatarImage)
BRIDGE_DECL(sceNpLookupAvatarImageAsync)
BRIDGE_DECL(sceNpLookupCreateRequest)
BRIDGE_DECL(sceNpLookupCreateTitleCtx)
BRIDGE_DECL(sceNpLookupDeleteRequest)
BRIDGE_DECL(sceNpLookupDeleteTitleCtx)
BRIDGE_DECL(sceNpLookupInit)
BRIDGE_DECL(sceNpLookupNpId)
BRIDGE_DECL(sceNpLookupNpIdAsync)
BRIDGE_DECL(sceNpLookupPollAsync)
BRIDGE_DECL(sceNpLookupSetTimeout)
BRIDGE_DECL(sceNpLookupTerm)
BRIDGE_DECL(sceNpLookupUserProfile)
BRIDGE_DECL(sceNpLookupUserProfileAsync)
BRIDGE_DECL(sceNpLookupWaitAsync)
BRIDGE_DECL(sceNpWordFilterAbortRequest)
BRIDGE_DECL(sceNpWordFilterCensorComment)
BRIDGE_DECL(sceNpWordFilterCensorCommentAsync)
BRIDGE_DECL(sceNpWordFilterCreateRequest)
BRIDGE_DECL(sceNpWordFilterCreateTitleCtx)
BRIDGE_DECL(sceNpWordFilterDeleteRequest)
BRIDGE_DECL(sceNpWordFilterDeleteTitleCtx)
BRIDGE_DECL(sceNpWordFilterInit)
BRIDGE_DECL(sceNpWordFilterPollAsync)
BRIDGE_DECL(sceNpWordFilterSanitizeComment)
BRIDGE_DECL(sceNpWordFilterSanitizeCommentAsync)
BRIDGE_DECL(sceNpWordFilterSetTimeout)
BRIDGE_DECL(sceNpWordFilterTerm)
BRIDGE_DECL(sceNpWordFilterWaitAsync)
