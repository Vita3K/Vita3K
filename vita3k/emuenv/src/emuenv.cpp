// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <emuenv/state.h>

#include <audio/state.h>
#include <config/state.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <gxm/state.h>
#include <http/state.h>
#include <ime/state.h>
#include <io/state.h>
#include <kernel/state.h>
#include <motion/state.h>
#include <net/state.h>
#include <ngs/state.h>
#include <nids/types.h>
#include <np/state.h>
#include <packages/sfo.h>
#include <regmgr/state.h>
#include <renderer/state.h>
#include <touch/state.h>

#include <gdbstub/state.h>

// initialize the unique_ptr then the reference each time
// this is VERY repetitive
EmuEnvState::EmuEnvState()
    : _app_info(new sfo::SfoAppInfo)
    , app_info(*_app_info)
    , _cfg(new Config)
    , cfg(*_cfg)
    , _mem(new MemState)
    , mem(*_mem)
    , _ctrl(new CtrlState)
    , ctrl(*_ctrl)
    , _touch(new TouchState)
    , touch(*_touch)
    , _kernel(new KernelState)
    , kernel(*_kernel)
    , _audio(new AudioState)
    , audio(*_audio)
    , _gxm(new GxmState)
    , gxm(*_gxm)
    , _io(new IOState)
    , io(*_io)
    , _motion(new MotionState)
    , motion(*_motion)
    , _net(new NetState)
    , net(*_net)
    , _netctl(new NetCtlState)
    , netctl(*_netctl)
    , _ngs(new ngs::State)
    , ngs(*_ngs)
    , _np(new NpState)
    , np(*_np)
    , _display(new DisplayState)
    , display(*_display)
    , _common_dialog(new DialogState)
    , common_dialog(*_common_dialog)
    , _ime(new Ime)
    , ime(*_ime)
    , _regmgr(new RegMgrState)
    , regmgr(*_regmgr)
    , _sfo_handle(new SfoFile)
    , sfo_handle(*_sfo_handle)
    , _gdb(new GDBState)
    , gdb(*_gdb)
    , _http(new HTTPState)
    , http(*_http) {
}

// this is necessary to forward declare unique_ptrs (so that they can call the appropriate destructor)
EmuEnvState::~EmuEnvState() = default;
