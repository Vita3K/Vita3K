// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

/**
 * @file SceSsl_tracy.cpp
 * @brief Tracy argument serializing functions for SceSsl
 */

#include <sstream>
#include <string>
#ifdef TRACY_ENABLE

#include "SceSsl_tracy.h"

void tracy_sceSslInit(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceSize poolSize) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string poolSize = "poolSize: ";
        } args;
    } tracy_settings;

    // Collect function call args to be logged
    tracy_settings.args.poolSize += std::to_string(poolSize);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.poolSize.c_str(), tracy_settings.args.poolSize.size());
}

void tracy_sceSslTerm(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
        } args;
    } tracy_settings;

    // Nothing to send ¯\_(ツ)_/¯
}
#endif // TRACY_ENABLE
