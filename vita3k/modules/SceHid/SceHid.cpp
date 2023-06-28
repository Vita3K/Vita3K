// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include "SceHid.h"

#include <SDL.h>
#include <SDL_keyboard.h>

#include <kernel/callback.h>
#include <kernel/state.h>
#include <util/lock_and_find.h>

#include "../SceKernelThreadMgr/SceThreadmgr.h"

EXPORT(int, sceHidConsumerControlEnumerate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidConsumerControlRead) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidConsumerControlRegisterEnumHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidConsumerControlRegisterReadHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidConsumerControlUnregisterEnumHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidConsumerControlUnregisterReadHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidControllerEnumerate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidControllerRead) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidControllerRegisterEnumHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidControllerRegisterReadHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidControllerUnregisterEnumHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidControllerUnregisterReadHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidKeyboardClear) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidKeyboardEnumerate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidKeyboardGetIntercept) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidKeyboardPeek) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidKeyboardRead) {
    return UNIMPLEMENTED();
}

int hid_thread_id = 0;
std::map<SceUID, CallbackPtr> hid_callbacks{};

static int SDLCALL thread_function(EmuEnvState &emuenv) {
    while (true) {
        int x, y;
        /* Uint32 buttons = SDL_GetMouseState(&x, &y);
        if ((buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0) {
            LOG_DEBUG("click detected");
            for (auto& cb : hid_callbacks) {
                cb.second->direct_notify(0);
                        }
                }*/
        // const uint8_t *keys = SDL_GetKeyboardState(nullptr);
        /*if (keys[emuenv.cfg.keyboard_button_cross]) {
            for (auto &cb : hid_callbacks) {
                cb.second->direct_notify(0);
            }
            SDL_GetMouseState()
        }*/
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

EXPORT(int, sceHidKeyboardRegisterEnumHintCallback, SceUID cbid) {
    LOG_DEBUG("dbid: {}", cbid);
    const auto cb = lock_and_find(cbid, emuenv.kernel.callbacks, emuenv.kernel.mutex);
    if (!cb)
        return RET_ERROR(-1);
    hid_callbacks[cbid] = cb;
    LOG_DEBUG("name:{}, owner_thread_id:{}", cb->get_name(), cb->get_owner_thread_id());
    auto hid_thread = std::thread(thread_function, std::ref(emuenv));
    hid_thread.detach();
    return 0;
}

EXPORT(int, sceHidKeyboardRegisterReadHintCallback, SceUID cbid) {
    LOG_DEBUG("dbid: {}", cbid);
    const auto cb = lock_and_find(cbid, emuenv.kernel.callbacks, emuenv.kernel.mutex);
    if (!cb)
        return RET_ERROR(-1);
    LOG_DEBUG("name:{}, owner_thread_id:{}", cb->get_name(), cb->get_owner_thread_id());
    hid_callbacks[cbid] = cb;
    auto hid_thread = std::thread(thread_function, std::ref(emuenv));
    hid_thread.detach();
    return 0;
}

EXPORT(int, sceHidKeyboardSetIntercept) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidKeyboardUnregisterEnumHintCallback, SceUID cbid) {
    LOG_DEBUG("dbid: {}", cbid);
    const auto cb = lock_and_find(cbid, emuenv.kernel.callbacks, emuenv.kernel.mutex);
    if (!cb)
        return RET_ERROR(-1);
    LOG_DEBUG("name:{}, owner_thread_id:{}", cb->get_name(), cb->get_owner_thread_id());
    hid_callbacks[cbid] = cb;
    auto hid_thread = std::thread(thread_function, std::ref(emuenv));
    hid_thread.detach();
    return 0;
}

EXPORT(int, sceHidKeyboardUnregisterReadHintCallback, SceUID cbid) {
    LOG_DEBUG("dbid: {}", cbid);
    const auto cb = lock_and_find(cbid, emuenv.kernel.callbacks, emuenv.kernel.mutex);
    if (!cb)
        return RET_ERROR(-1);
    LOG_TRACE("name:{}, owner_thread_id:{}", cb->get_name(), cb->get_owner_thread_id());
    hid_callbacks[cbid] = cb;
    // SDL_CreateThread(&thread_function, "SceGxmDisplayQueue",(void*)emuenv*);
    auto hid_thread = std::thread(thread_function, std::ref(emuenv));
    hid_thread.detach();
    return 0;
}

EXPORT(int, sceHidMouseEnumerate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidMouseRead) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidMouseRegisterEnumHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidMouseRegisterReadHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidMouseUnregisterEnumHintCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceHidMouseUnregisterReadHintCallback) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceHidConsumerControlEnumerate)
BRIDGE_IMPL(sceHidConsumerControlRead)
BRIDGE_IMPL(sceHidConsumerControlRegisterEnumHintCallback)
BRIDGE_IMPL(sceHidConsumerControlRegisterReadHintCallback)
BRIDGE_IMPL(sceHidConsumerControlUnregisterEnumHintCallback)
BRIDGE_IMPL(sceHidConsumerControlUnregisterReadHintCallback)
BRIDGE_IMPL(sceHidControllerEnumerate)
BRIDGE_IMPL(sceHidControllerRead)
BRIDGE_IMPL(sceHidControllerRegisterEnumHintCallback)
BRIDGE_IMPL(sceHidControllerRegisterReadHintCallback)
BRIDGE_IMPL(sceHidControllerUnregisterEnumHintCallback)
BRIDGE_IMPL(sceHidControllerUnregisterReadHintCallback)
BRIDGE_IMPL(sceHidKeyboardClear)
BRIDGE_IMPL(sceHidKeyboardEnumerate)
BRIDGE_IMPL(sceHidKeyboardGetIntercept)
BRIDGE_IMPL(sceHidKeyboardPeek)
BRIDGE_IMPL(sceHidKeyboardRead)
BRIDGE_IMPL(sceHidKeyboardRegisterEnumHintCallback)
BRIDGE_IMPL(sceHidKeyboardRegisterReadHintCallback)
BRIDGE_IMPL(sceHidKeyboardSetIntercept)
BRIDGE_IMPL(sceHidKeyboardUnregisterEnumHintCallback)
BRIDGE_IMPL(sceHidKeyboardUnregisterReadHintCallback)
BRIDGE_IMPL(sceHidMouseEnumerate)
BRIDGE_IMPL(sceHidMouseRead)
BRIDGE_IMPL(sceHidMouseRegisterEnumHintCallback)
BRIDGE_IMPL(sceHidMouseRegisterReadHintCallback)
BRIDGE_IMPL(sceHidMouseUnregisterEnumHintCallback)
BRIDGE_IMPL(sceHidMouseUnregisterReadHintCallback)
