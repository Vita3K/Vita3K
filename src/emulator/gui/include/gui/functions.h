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

#include <host/app.h>

#include <cstdint>

static constexpr auto MENUBAR_HEIGHT = 19;

enum GenericDialogState {
    UNK_STATE,
    CONFIRM_STATE,
    CANCEL_STATE
};

struct HostState;

void DrawMainMenuBar(HostState &host);
void DrawThreadsDialog(HostState &host);
void DrawSemaphoresDialog(HostState &host);
void DrawMutexesDialog(HostState &host);
void DrawLwMutexesDialog(HostState &host);
void DrawLwCondvarsDialog(HostState &host);
void DrawCondvarsDialog(HostState &host);
void DrawEventFlagsDialog(HostState &host);
void DrawUI(HostState &host);
void DrawCommonDialog(HostState &host);
void DrawGameSelector(HostState &host, AppRunType *run_type);
void DrawReinstallDialog(HostState &host, GenericDialogState *status);
void DrawControlsDialog(HostState &host);
