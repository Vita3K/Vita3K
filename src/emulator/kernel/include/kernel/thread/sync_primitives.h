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

#include <psp2/types.h>

#include <kernel/state.h>

enum class SyncWeight {
    Light, // lightweight
    Heavy // 'heavy'weight
};

// Mutex
SceUID mutex_create(SceUID *uid_out, KernelState &kernel, const char *export_name, SceUID thread_id, const char *name, SceUInt attr, int init_count, SyncWeight weight);
int mutex_lock(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, int lock_count, unsigned int *timeout, SyncWeight weight);
int mutex_unlock(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, int unlock_count, SyncWeight weight);
int mutex_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, SyncWeight weight);

// Semaphore
SceUID semaphore_create(KernelState &kernel, const char *export_name, const char *name, SceUInt attr, int initVal, int maxVal);
int semaphore_wait(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid, int signal, SceUInt *timeout);
int semaphore_signal(KernelState &kernel, const char *export_name, SceUID semaid, int signal);

int wait_semaphore(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid, int signal, SceUInt *timeout);
int signal_sema(KernelState &kernel, const char *export_name, SceUID semaid, int signal);
