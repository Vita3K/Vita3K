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

#include <module/module.h>

#include <kernel/state.h>
#include <io/functions.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceMusicExport);

#define SCE_MUSIC_EXPORT_MAX_FS_PATH 1024
#define SCE_MUSIC_EXPORT_MAX_MEMBLOCK_SIZE 65536

enum SceMusicExportError {
    SCE_MUSIC_EXPORT_ERROR_PARAM               = 0x80105301,
    SCE_MUSIC_EXPORT_ERROR_FILE_NOT_FOUND      = 0x80105302,
    SCE_MUSIC_EXPORT_ERROR_CONTENT_FULL        = 0x80105303,
    SCE_MUSIC_EXPORT_ERROR_NO_MEMORY           = 0x80105304,
    SCE_MUSIC_EXPORT_ERROR_SERVER_DOWN         = 0x80105305,
    SCE_MUSIC_EXPORT_ERROR_TOO_MANY_CLIENT     = 0x80105306,
    SCE_MUSIC_EXPORT_ERROR_MEDIA_FULL          = 0x80105307,
    SCE_MUSIC_EXPORT_ERROR_CREATE_FILE         = 0x80105308,
    SCE_MUSIC_EXPORT_ERROR_NOT_SUPPORTED_FORMAT= 0x80105309,
    SCE_MUSIC_EXPORT_ERROR_CANCELED            = 0x8010530a,
    SCE_MUSIC_EXPORT_ERROR_INTERNAL            = 0x8010530b,
    SCE_MUSIC_EXPORT_ERROR_MEDIA_NOT_EXIST     = 0x8010530c,
    SCE_MUSIC_EXPORT_ERROR_DB_CORRUPTED        = 0x8010530d,
    SCE_MUSIC_EXPORT_ERROR_INVALID_PATH        = 0x8010530e,
    SCE_MUSIC_EXPORT_ERROR_FILE_EXIST = 0x8010530f,
};

struct SceMusicExportParam {
    uint8_t reserved[128];
};

typedef SceBool (*SceMusicExportCancelFunc)(
    void *userdata);

typedef void (*SceMusicExportProgressInfoFunc)(
    void *userdata,
    SceUInt32 progress);

EXPORT(int, sceMusicExportFromFile,
    const char *musicfilePath,
    const SceMusicExportParam *param,
    void *workMemory,
    SceMusicExportCancelFunc cancelFunc,
    SceMusicExportProgressInfoFunc progressFunc,
    Ptr<void> userdata,
    char *exportedPath,
    int exportedPathLength) {

    if (!musicfilePath || !exportedPath || exportedPathLength <= 0)
        return SCE_MUSIC_EXPORT_ERROR_PARAM;

    const int src_fd = open_file(emuenv.io, musicfilePath, SCE_O_RDONLY, emuenv.pref_path, export_name);
    if (src_fd < 0)
        return SCE_MUSIC_EXPORT_ERROR_FILE_NOT_FOUND;
    LOG_DEBUG("Opened source file: {} with fd {}", musicfilePath, src_fd);
    std::string dst_path = "ux0:/music/";
    dst_path += fs::path(musicfilePath).filename().string();

    const int dst_fd = open_file(emuenv.io, dst_path.c_str(),
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
        emuenv.pref_path, export_name);
    LOG_DEBUG("Opened destination file: {} with fd {}", dst_path, dst_fd);

    if (dst_fd < 0)
        return SCE_MUSIC_EXPORT_ERROR_CREATE_FILE;

    char internal_work[SCE_MUSIC_EXPORT_MAX_MEMBLOCK_SIZE];
    SceUInt32 work_size = SCE_MUSIC_EXPORT_MAX_MEMBLOCK_SIZE;

    if (workMemory) {
        work_size = SCE_MUSIC_EXPORT_MAX_MEMBLOCK_SIZE;
    } else {
        workMemory = internal_work;
    }
    SceUInt64 total = 0;

    SceIoStat stat;
    if (stat_file_by_fd(emuenv.io, src_fd, &stat, emuenv.pref_path, export_name) < 0) {
        close_file(emuenv.io, src_fd, export_name);
        close_file(emuenv.io, dst_fd, export_name);
        return SCE_MUSIC_EXPORT_ERROR_INTERNAL;
    }

    LOG_DEBUG("File size to export: {} bytes", stat.st_size);

    while (true) {
        LOG_DEBUG("Starting read/write iteration, total bytes copied: {}", total);
        if (cancelFunc && cancelFunc(userdata.get(emuenv.mem))) {
            close_file(emuenv.io, src_fd, export_name);
            close_file(emuenv.io, dst_fd, export_name);
            return SCE_MUSIC_EXPORT_ERROR_CANCELED;
        }

        int read_bytes = read_file(workMemory, emuenv.io, src_fd, work_size, export_name);
        if (read_bytes < 0) {
            close_file(emuenv.io, src_fd, export_name);
            close_file(emuenv.io, dst_fd, export_name);
            return SCE_MUSIC_EXPORT_ERROR_INTERNAL;
        }
        LOG_DEBUG("Read {} bytes from fd {}", read_bytes, src_fd);

        if (read_bytes == 0)
            break;

        int written = write_file(dst_fd, workMemory, read_bytes, emuenv.io, export_name);
        LOG_DEBUG("Wrote {} bytes to fd {}", written, dst_fd);
        if (written != read_bytes) {
            close_file(emuenv.io, src_fd, export_name);
            close_file(emuenv.io, dst_fd, export_name);
            return SCE_MUSIC_EXPORT_ERROR_INTERNAL;
        }

        total += read_bytes;
        SceUInt32 percent = static_cast<SceUInt32>((total * 100) / stat.st_size);
        if (percent > 100)
            percent = 100;

        LOG_DEBUG("Total bytes copied so far: {}", total);
        if (progressFunc) {
            auto thread = emuenv.kernel.get_thread(thread_id);
            uintptr_t cb_addr = reinterpret_cast<uintptr_t>(progressFunc);
            thread->run_callback(cb_addr, { userdata.address(), static_cast<uint64_t>(percent) });
            LOG_DEBUG("Called progress callback with {}%", percent);
        }
        LOG_DEBUG("Completed read/write iteration, total bytes copied: {}", total);
    }

    LOG_DEBUG("Completed file export, total bytes copied: {}", total);
    close_file(emuenv.io, src_fd, export_name);
    close_file(emuenv.io, dst_fd, export_name);

    strncpy(exportedPath, dst_path.c_str(), exportedPathLength - 1);
    exportedPath[exportedPathLength - 1] = '\0';
    LOG_DEBUG("Exported file path: {}", exportedPath);

    return 0;
}
