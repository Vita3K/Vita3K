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

#include <util/types.h>

enum SceCtrlErrorCode {
    SCE_CTRL_ERROR_INVALID_ARG = 0x80340001,
    SCE_CTRL_ERROR_PRIV_REQUIRED = 0x80340002,
    SCE_CTRL_ERROR_NO_DEVICE = 0x80340020,
    SCE_CTRL_ERROR_NOT_SUPPORTED = 0x80340021,
    SCE_CTRL_ERROR_INVALID_MODE = 0x80340022,
    SCE_CTRL_ERROR_FATAL = 0x803400FF
};

enum SceCtrlButtons {
    SCE_CTRL_SELECT = 0x00000001, //!< Select button.
    SCE_CTRL_L3 = 0x00000002, //!< L3 button.
    SCE_CTRL_R3 = 0x00000004, //!< R3 button.
    SCE_CTRL_START = 0x00000008, //!< Start button.
    SCE_CTRL_UP = 0x00000010, //!< Up D-Pad button.
    SCE_CTRL_RIGHT = 0x00000020, //!< Right D-Pad button.
    SCE_CTRL_DOWN = 0x00000040, //!< Down D-Pad button.
    SCE_CTRL_LEFT = 0x00000080, //!< Left D-Pad button.
    SCE_CTRL_LTRIGGER = 0x00000100, //!< Left trigger.
    SCE_CTRL_L2 = SCE_CTRL_LTRIGGER, //!< L2 button.
    SCE_CTRL_RTRIGGER = 0x00000200, //!< Right trigger.
    SCE_CTRL_R2 = SCE_CTRL_RTRIGGER, //!< R2 button.
    SCE_CTRL_L1 = 0x00000400, //!< L1 button.
    SCE_CTRL_R1 = 0x00000800, //!< R1 button.
    SCE_CTRL_TRIANGLE = 0x00001000, //!< Triangle button.
    SCE_CTRL_CIRCLE = 0x00002000, //!< Circle button.
    SCE_CTRL_CROSS = 0x00004000, //!< Cross button.
    SCE_CTRL_SQUARE = 0x00008000, //!< Square button.
    SCE_CTRL_INTERCEPTED = 0x00010000, //!< Input not available because intercepted by another application
    SCE_CTRL_PSBUTTON = SCE_CTRL_INTERCEPTED, //!< Playstation (Home) button.
    SCE_CTRL_HEADPHONE = 0x00080000, //!< Headphone plugged in.
    SCE_CTRL_VOLUP = 0x00100000, //!< Volume up button.
    SCE_CTRL_VOLDOWN = 0x00200000, //!< Volume down button.
    SCE_CTRL_POWER = 0x40000000 //!< Power button.
};

enum SceCtrlPadInputMode {
    SCE_CTRL_MODE_DIGITAL = 0,
    SCE_CTRL_MODE_ANALOG = 1,
    SCE_CTRL_MODE_ANALOG_WIDE = 2
};

struct SceCtrlData {
    SceUInt64 timeStamp; //!< time stamp (usec)
    SceUInt32 buttons; //!< pressed digital button
    SceUInt8 lx; //!< L analog controller (X-axis)
    SceUInt8 ly; //!< L analog controller (Y-axis)
    SceUInt8 rx; //!< R analog controller (X-axis)
    SceUInt8 ry; //!< R analog controller (Y-axis)
    SceUInt8 rsrv[16]; //!< reserved
};

struct SceCtrlData2 {
    SceUInt64 timeStamp; //!< time stamp (usec)
    SceUInt32 buttons; //!< pressed digital button
    SceUInt8 lx; //!< L analog controller (X-axis)
    SceUInt8 ly; //!< L analog controller (Y-axis)
    SceUInt8 rx; //!< R analog controller (X-axis)
    SceUInt8 ry; //!< R analog controller (Y-axis)
    SceUInt8 up; //!< Up press value
    SceUInt8 right; ///!< Right epress value
    SceUInt8 down; //!< Down press value
    SceUInt8 left; //!< Left press value
    SceUInt8 l2; //!< L2 press value
    SceUInt8 r2; //!< R2 press value
    SceUInt8 l1; //!< L1 press value
    SceUInt8 r1; //!< R1 press value
    SceUInt8 triangle; //!< Triangle press value
    SceUInt8 circle; //!< Circle press value
    SceUInt8 cross; //!< Cross press value
    SceUInt8 square; //!< Square press value
    SceUInt8 rsrv[4]; //!< reserved
};

#define SCE_CTRL_MAX_WIRELESS_NUM 4

enum SceCtrlWirelessInfo {
    SCE_CTRL_WIRELESS_INFO_NOT_CONNECTED = 0,
    SCE_CTRL_WIRELESS_INFO_CONNECTED = 1
};

struct SceCtrlWirelessControllerInfo {
    SceUInt8 connected[SCE_CTRL_MAX_WIRELESS_NUM]; //!< 0:not connected, 1:connected
    SceUInt8 reserved[12]; //!< reserved
};

enum SceCtrlExternalInputMode {
    SCE_CTRL_TYPE_UNPAIRED = 0, //!< Unpaired controller
    SCE_CTRL_TYPE_PHY = 1, //!< Physical controller for VITA
    SCE_CTRL_TYPE_VIRT = 2, //!< Virtual controller for PSTV
    SCE_CTRL_TYPE_DS3 = 4, //!< DualShock 3
    SCE_CTRL_TYPE_DS4 = 8 //!< DualShock 4
};

struct SceCtrlPortInfo {
    uint8_t port[5]; //!< Controller type of each port (See ::SceCtrlExternalInputMode)
    uint8_t unk[11]; //!< Unknown
};

struct SceCtrlActuator {
    unsigned char small; //!< Vibration strength of the small motor
    unsigned char large; //!< Vibration strength of the large motor
    uint8_t unk[6]; //!< Unknown
};
