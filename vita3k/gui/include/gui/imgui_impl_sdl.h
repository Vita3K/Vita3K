// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <emuenv/window.h>
#include <gui/imgui_impl_sdl_state.h>

union SDL_Event;
struct SDL_Window;
struct SDL_Cursor;

IMGUI_API ImGui_State *ImGui_ImplSdl_Init(renderer::State *renderer, SDL_Window *window);
IMGUI_API void ImGui_ImplSdl_Shutdown(ImGui_State *state);
IMGUI_API void ImGui_ImplSdl_NewFrame(ImGui_State *state);
IMGUI_API void ImGui_ImplSdl_RenderDrawData(ImGui_State *state);
IMGUI_API bool ImGui_ImplSdl_ProcessEvent(ImGui_State *state, SDL_Event *event);

IMGUI_API ImTextureID ImGui_ImplSdl_CreateTexture(ImGui_State *state, void *data, int width, int height);
IMGUI_API void ImGui_ImplSdl_DeleteTexture(ImGui_State *state, ImTextureID texture);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdl_InvalidateDeviceObjects(ImGui_State *state);
IMGUI_API bool ImGui_ImplSdl_CreateDeviceObjects(ImGui_State *state);
