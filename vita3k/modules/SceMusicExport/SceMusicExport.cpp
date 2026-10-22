// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <module/module.h>

#include "../SceLibKernel/SceLibKernel.h"

#include <io/device.h>
#include <io/functions.h>
#include <io/state.h>
#include <io/types.h>
#include <util/fs.h>
#include <util/log.h>

#include <boost/filesystem/operations.hpp>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceMusicExport);

typedef struct sceMusicExportParam {
    uint8_t reserved[128]; //!< Reserved data
} sceMusicExportParam;
static_assert(sizeof(sceMusicExportParam) == 128, "sceMusicExportParam must be 128 bytes");

enum SceMusicExportError {
    SCE_MUSIC_EXPORT_ERROR_INVALID_PARAMS = 0x80105301,
    SCE_MUSIC_EXPORT_ERROR_FILE_NOT_FOUND = 0x80105302,
    SCE_MUSIC_EXPORT_ERROR_INVALID_PATH = 0x8010530e,
    SCE_MUSIC_EXPORT_ERROR_FILE_INVALID_FORMAT = 0x80105309,
};

EXPORT(int, sceMusicExportFromFile, const char *path, const sceMusicExportParam *param, void *workingMemory, void *cancelCb, void (*progress)(void *, int), void *userData, char *outPath, SceSize outPathSize) {
    TRACY_FUNC(sceMusicExportFromFile, path, param, workingMemory, cancelCb, progress, userData, outPath, outPathSize);

    if (!path || !outPath || outPathSize == 0)
        return RET_ERROR(SCE_MUSIC_EXPORT_ERROR_INVALID_PARAMS);

    const auto path_len = strnlen(path, 1024);

    if (path_len == 1024)
        return RET_ERROR(SCE_MUSIC_EXPORT_ERROR_INVALID_PATH);

    // some check with the registry goes here

    // This check is only done if the param is provided and the sdk version is higher enough, dunno why
    if (param) {
        for (uint8_t i = 0; i < sizeof(sceMusicExportParam); i++) {
            if (param->reserved[i] != 0)
                return RET_ERROR(SCE_MUSIC_EXPORT_ERROR_INVALID_PARAMS);
        }
    }

    SceIoStat stat;

    const auto io_ret = CALL_EXPORT(sceIoGetstat, path, &stat);

    if (io_ret != 0)
        return RET_ERROR(SCE_MUSIC_EXPORT_ERROR_FILE_NOT_FOUND);

    if (stat.st_size > 0x7fffffff)
        return RET_ERROR(SCE_MUSIC_EXPORT_ERROR_FILE_INVALID_FORMAT);

    const std::string_view path_view(path, path_len);

    bool valid_extension = false;
    static constexpr std::array<std::string_view, 4> valid_extensions = { ".mp3", ".m4a", ".3gp", ".wav" };
    for (const auto &ext : valid_extensions) {
        if (path_view.ends_with(ext)) {
            valid_extension = true;
            break;
        }
    }

    if (!valid_extension)
        return RET_ERROR(SCE_MUSIC_EXPORT_ERROR_FILE_INVALID_FORMAT);

    LOG_WARN_ONCE("Ignoring work memory and callbacks");

    const auto fd = CALL_EXPORT(sceIoOpen, path, SCE_O_RDONLY, 0);
    if (fd < 0)
        return RET_ERROR(SCE_MUSIC_EXPORT_ERROR_FILE_NOT_FOUND);

    const auto file_size = CALL_EXPORT(sceIoLseek, fd, 0, SCE_SEEK_END);
    CALL_EXPORT(sceIoClose, fd);

    if (file_size == 0) {
        // TODO: Execute progress callback at 100%
        // TODO: What to do with outPath here?
        return 0; // Assume it was exported correctly
    }

    const auto dst_path = emuenv.shared_path / "exported-music" / emuenv.io.title_id;

    fs::create_directories(dst_path);

    auto device = device::get_device(path);
    auto translated_path = translate_path(path, device, emuenv.io.device_paths);

    const auto device_string = device::get_device_string(device);

    const auto finished_path = emuenv.vita_fs_path / device_string / translated_path;

    // Copy the file to the destination path
    LOG_DEBUG("Exporting music file \"{}\" to \"{}\"", path, fs_utils::path_to_utf8(dst_path));
    const auto dst_file_path = dst_path / fs_utils::utf8_to_path(path).filename();
    fs::copy_file(finished_path, dst_file_path, fs::copy_options::overwrite_existing);

    LOG_WARN_ONCE("Setting outPath is not implemented"); // not sure if game can handle non-partitioned paths
    outPath[0] = '\0';

    return 0;
}
