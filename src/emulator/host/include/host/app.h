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

#include <glbinding/gl/types.h>

#include <memory>
#include <string>
#include <vector>

static constexpr auto DEFAULT_RES_WIDTH = 960;
static constexpr auto DEFAULT_RES_HEIGHT = 544;

struct HostState;
struct SDL_Window;
struct SDL_Surface;
template <class T>
class Ptr;

typedef std::unique_ptr<SDL_Surface, void (*)(SDL_Surface *)> SurfacePtr;

enum ExitCode {
    Success = 0,
    IncorrectArgs,
    SDLInitFailed,
    HostInitFailed,
    RendererInitFailed,
    ModuleLoadFailed,
    InitThreadFailed,
    RunThreadFailed,
    InvalidApplicationPath,
    QuitRequested
};

/// Describes the state of the application to be run
enum class AppRunType {
    /// Run type is unknown
    Unknown,
    /// Extracted, files are as they are on console
    Extracted,
    /// Zipped in HENKaku-style .vpk file
    Vpk,
};

void error_dialog(const std::string &message, SDL_Window *window = nullptr);
ExitCode load_app(Ptr<const void> &entry_point, HostState &host, const std::wstring &path, AppRunType run_type);
ExitCode run_app(HostState &host, Ptr<const void> &entry_point);

void set_window_title(HostState &host);
