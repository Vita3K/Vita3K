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

#pragma once

#include <module/module.h>

// SceNpCommerce2
BRIDGE_DECL(sceNpCommerce2AbortReq)
BRIDGE_DECL(sceNpCommerce2CreateCtx)
BRIDGE_DECL(sceNpCommerce2CreateSessionCreateReq)
BRIDGE_DECL(sceNpCommerce2CreateSessionGetResult)
BRIDGE_DECL(sceNpCommerce2CreateSessionStart)
BRIDGE_DECL(sceNpCommerce2DestroyCtx)
BRIDGE_DECL(sceNpCommerce2DestroyGetCategoryContentsResult)
BRIDGE_DECL(sceNpCommerce2DestroyGetProductInfoListResult)
BRIDGE_DECL(sceNpCommerce2DestroyGetProductInfoResult)
BRIDGE_DECL(sceNpCommerce2DestroyReq)
BRIDGE_DECL(sceNpCommerce2GetCategoryContentsCreateReq)
BRIDGE_DECL(sceNpCommerce2GetCategoryContentsGetResult)
BRIDGE_DECL(sceNpCommerce2GetCategoryContentsStart)
BRIDGE_DECL(sceNpCommerce2GetCategoryInfo)
BRIDGE_DECL(sceNpCommerce2GetCategoryInfoFromContentInfo)
BRIDGE_DECL(sceNpCommerce2GetContentInfo)
BRIDGE_DECL(sceNpCommerce2GetContentRatingDescriptor)
BRIDGE_DECL(sceNpCommerce2GetContentRatingInfoFromCategoryInfo)
BRIDGE_DECL(sceNpCommerce2GetContentRatingInfoFromGameProductInfo)
BRIDGE_DECL(sceNpCommerce2GetGameProductInfo)
BRIDGE_DECL(sceNpCommerce2GetGameProductInfoFromContentInfo)
BRIDGE_DECL(sceNpCommerce2GetGameProductInfoFromGetProductInfoListResult)
BRIDGE_DECL(sceNpCommerce2GetGameSkuInfoFromGameProductInfo)
BRIDGE_DECL(sceNpCommerce2GetPrice)
BRIDGE_DECL(sceNpCommerce2GetProductInfoCreateReq)
BRIDGE_DECL(sceNpCommerce2GetProductInfoGetResult)
BRIDGE_DECL(sceNpCommerce2GetProductInfoListCreateReq)
BRIDGE_DECL(sceNpCommerce2GetProductInfoListGetResult)
BRIDGE_DECL(sceNpCommerce2GetProductInfoListStart)
BRIDGE_DECL(sceNpCommerce2GetProductInfoStart)
BRIDGE_DECL(sceNpCommerce2GetSessionInfo)
BRIDGE_DECL(sceNpCommerce2GetShortfallOfLibhttpPool)
BRIDGE_DECL(sceNpCommerce2GetShortfallOfLibsslPool)
BRIDGE_DECL(sceNpCommerce2HidePsStoreIcon)
BRIDGE_DECL(sceNpCommerce2Init)
BRIDGE_DECL(sceNpCommerce2InitGetCategoryContentsResult)
BRIDGE_DECL(sceNpCommerce2InitGetProductInfoListResult)
BRIDGE_DECL(sceNpCommerce2InitGetProductInfoResult)
BRIDGE_DECL(sceNpCommerce2ShowPsStoreIcon)
BRIDGE_DECL(sceNpCommerce2StartEmptyStoreCheck)
BRIDGE_DECL(sceNpCommerce2StopEmptyStoreCheck)
BRIDGE_DECL(sceNpCommerce2Term)
