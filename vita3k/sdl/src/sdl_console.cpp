// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl/sdl_console.h>

#if defined(_WIN32) && !defined(__ANDROID__)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdio>

namespace sdl {

void init_console() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        AllocConsole();

    FILE *stream = nullptr;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
}

} // namespace sdl
#endif