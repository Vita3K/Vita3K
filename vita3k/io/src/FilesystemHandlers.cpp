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

#include "io/FilesytemHandlers.h"

/**
 * Static methods should be defined outside the class.
 */

FilesystemHandlers *FilesystemHandlers::my_instance = nullptr;

/**
 * The first time we call GetInstance we will lock the storage location
 *      and then we make sure again that the variable is null and then we
 *      set the value. RU:
 */
FilesystemHandlers *FilesystemHandlers::GetInstance(const fs::path &base_path) {
    if (my_instance == nullptr) {
        my_instance = new FilesystemHandlers(base_path);
    }
    return my_instance;
}

AbstractFS *FilesystemHandlers::open_filesystem(VitaIoDevice device) const {
    // TODO: Implement device mounts unique access
    switch (device) {
    case +VitaIoDevice::gro0:
    case +VitaIoDevice::grw0:
    case +VitaIoDevice::app0:
    case +VitaIoDevice::ux0: return new UX0(file_base_path);
    default: return new UX0(file_base_path);
    }
    
}
