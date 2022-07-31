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
 * @file SceTouch_tracy.cpp
 * @brief Tracy argument serializing functions for SceTouch
 */

#include "touch/touch.h"
#include <sstream>
#include <string>
#ifdef TRACY_ENABLE

#include "SceTouch_tracy.h"

void tracy_sceTouchGetPanelInfo(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchPanelInfo *pPanelInfo) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string port = "port: ";
            std::string pPanelInfo = "&pPanelInfo: ";
        } args;

    } tracy_settings;

    // Log enumerated args
    tracy_settings.args.port += std::to_string(port);

    std::stringstream pPanelInfoss;
    pPanelInfoss << pPanelInfo;
    tracy_settings.args.pPanelInfo += pPanelInfoss.str();

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.pPanelInfo.c_str(), tracy_settings.args.pPanelInfo.size());
}

void tracy_sceTouchGetSamplingState(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchSamplingState *pState) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string port = "port: ";
            std::string pState = "&pState: ";
        } args;

    } tracy_settings;

    // Log enumerated args
    tracy_settings.args.port += std::to_string(port);

    std::stringstream pStatess;
    pStatess << pState;
    tracy_settings.args.pState += pStatess.str();

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.pState.c_str(), tracy_settings.args.pState.size());
}

void tracy_sceTouchPeek(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string port = "port: ";
            std::string pData = "&pData: ";
            std::string nBufs = "nBufs: ";
        } args;

    } tracy_settings;

    // Log enumerated args
    tracy_settings.args.port += std::to_string(port);

    std::stringstream pDatass;
    pDatass << pData;
    tracy_settings.args.pData += pDatass.str();
    tracy_settings.args.nBufs += std::to_string(nBufs);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.pData.c_str(), tracy_settings.args.pData.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.nBufs.c_str(), tracy_settings.args.nBufs.size());
}

void tracy_sceTouchPeek2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string port = "port: ";
            std::string pData = "&pData: ";
            std::string nBufs = "nBufs: ";
        } args;

    } tracy_settings;

    // Log enumerated args
    tracy_settings.args.port += std::to_string(port);

    std::stringstream pDatass;
    pDatass << pData;
    tracy_settings.args.pData += pDatass.str();
    tracy_settings.args.nBufs += std::to_string(nBufs);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.pData.c_str(), tracy_settings.args.pData.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.nBufs.c_str(), tracy_settings.args.nBufs.size());
}

void tracy_sceTouchRead(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string port = "port: ";
            std::string pData = "&pData: ";
            std::string nBufs = "nBufs: ";
        } args;

    } tracy_settings;

    // Log enumerated args
    tracy_settings.args.port += std::to_string(port);

    std::stringstream pDatass;
    pDatass << pData;
    tracy_settings.args.pData += pDatass.str();
    tracy_settings.args.nBufs += std::to_string(nBufs);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.pData.c_str(), tracy_settings.args.pData.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.nBufs.c_str(), tracy_settings.args.nBufs.size());
}

void tracy_sceTouchRead2(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string port = "port: ";
            std::string pData = "&pData: ";
            std::string nBufs = "nBufs: ";
        } args;

    } tracy_settings;

    // Log enumerated args
    tracy_settings.args.port += std::to_string(port);

    std::stringstream pDatass;
    pDatass << pData;
    tracy_settings.args.pData += pDatass.str();
    tracy_settings.args.nBufs += std::to_string(nBufs);

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.pData.c_str(), tracy_settings.args.pData.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.nBufs.c_str(), tracy_settings.args.nBufs.size());
}

void tracy_sceTouchSetSamplingState(tracy::ScopedZone *___tracy_scoped_zone, bool module_activation, SceUInt32 port, SceTouchSamplingState state) {
    // Tracy activation state check to log arguments
    if (!module_activation)
        return;

    struct TracyLogSettings {
        struct args {
            std::string port = "port: ";
            std::string state = "state: ";
        } args;

    } tracy_settings;

    // Log enumerated args
    tracy_settings.args.port += std::to_string(port);
    tracy_settings.args.state += state == SCE_TOUCH_SAMPLING_STATE_START ? "Start" : "Stop";

    // Send args
    ___tracy_scoped_zone->Text(tracy_settings.args.port.c_str(), tracy_settings.args.port.size());
    ___tracy_scoped_zone->Text(tracy_settings.args.state.c_str(), tracy_settings.args.state.size());
}

#endif // TRACY_ENABLE
