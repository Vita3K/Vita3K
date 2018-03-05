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

#include "SceLiveAreaUtil.h"

EXPORT(int, sceLiveAreaGetFrameRevision) {
    return unimplemented("sceLiveAreaGetFrameRevision");
}

EXPORT(int, sceLiveAreaGetFrameUserData) {
    return unimplemented("sceLiveAreaGetFrameUserData");
}

EXPORT(int, sceLiveAreaGetRevision) {
    return unimplemented("sceLiveAreaGetRevision");
}

EXPORT(int, sceLiveAreaGetStatus) {
    return unimplemented("sceLiveAreaGetStatus");
}

EXPORT(int, sceLiveAreaReplaceAllAsync) {
    return unimplemented("sceLiveAreaReplaceAllAsync");
}

EXPORT(int, sceLiveAreaReplaceAllSync) {
    return unimplemented("sceLiveAreaReplaceAllSync");
}

EXPORT(int, sceLiveAreaUpdateFrameAsync) {
    return unimplemented("sceLiveAreaUpdateFrameAsync");
}

EXPORT(int, sceLiveAreaUpdateFrameSync) {
    return unimplemented("sceLiveAreaUpdateFrameSync");
}

BRIDGE_IMPL(sceLiveAreaGetFrameRevision)
BRIDGE_IMPL(sceLiveAreaGetFrameUserData)
BRIDGE_IMPL(sceLiveAreaGetRevision)
BRIDGE_IMPL(sceLiveAreaGetStatus)
BRIDGE_IMPL(sceLiveAreaReplaceAllAsync)
BRIDGE_IMPL(sceLiveAreaReplaceAllSync)
BRIDGE_IMPL(sceLiveAreaUpdateFrameAsync)
BRIDGE_IMPL(sceLiveAreaUpdateFrameSync)
