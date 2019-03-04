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

#include <imgui.h>

struct HostState;

#define RGBA_TO_FLOAT(r, g, b, a) ImVec4(r / 255.0, g / 255.0, b / 255.0, a / 255.0)

const ImVec4 GUI_COLOR_TEXT_MENUBAR = RGBA_TO_FLOAT(242, 150, 58, 255);
const ImVec4 GUI_COLOR_TEXT_MENUBAR_OPTIONS = RGBA_TO_FLOAT(242, 150, 58, 255);
const ImVec4 GUI_COLOR_TEXT_TITLE = RGBA_TO_FLOAT(247, 198, 51, 255);
const ImVec4 GUI_COLOR_TEXT = RGBA_TO_FLOAT(255, 255, 255, 255);

#undef RGBA_TO_FLOAT

void DrawMainMenuBar(HostState &host);
void DrawThreadsDialog(HostState &host);
void DrawThreadDetailsDialog(HostState &host);
void DrawSemaphoresDialog(HostState &host);
void DrawMutexesDialog(HostState &host);
void DrawLwMutexesDialog(HostState &host);
void DrawLwCondvarsDialog(HostState &host);
void DrawCondvarsDialog(HostState &host);
void DrawEventFlagsDialog(HostState &host);
void DrawAllocationsDialog(HostState &host);
void DrawDisassemblyDialog(HostState &host);
void DrawControlsDialog(HostState &host);
void DrawAboutDialog(HostState &host);

void ReevaluateCode(HostState &host);
