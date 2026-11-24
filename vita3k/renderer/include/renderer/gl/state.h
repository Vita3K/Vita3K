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

#pragma once

#include <renderer/gl/compile_worker.h>
#include <renderer/gl/overlay_renderer.h>
#include <renderer/gl/screen_render.h>
#include <renderer/gl/surface_cache.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include "types.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace renderer::gl {

struct PendingReadback {
    GLuint pbo;
    size_t capacity;
    size_t size;
    GLsync fence;
    SceGxmColorSurface surface;
    uint32_t width;
    uint32_t height;
    int multiplier;
    bool use_temp;
    Address begin;
    Address end;
    SceGxmNotification vertex_notification;
    SceGxmNotification fragment_notification;
};

struct ReadbackBuffer {
    GLuint pbo;
    size_t capacity;
};

struct DownscaleTarget {
    uint32_t width;
    uint32_t height;
    GLenum internal_format;
    GLuint texture;
    GLuint framebuffer;
};

struct GLState : public renderer::State {
    ShaderCache fragment_shader_cache;
    ShaderCache vertex_shader_cache;
    ProgramCache program_cache;

    GLTextureCache texture_cache;
    GLSurfaceCache surface_cache;

    ScreenRenderer screen_renderer;
    OverlayRenderer overlay_renderer;

    bool context_is_current = false;

    std::deque<PendingReadback> pending_readbacks;
    std::vector<ReadbackBuffer> free_readback_buffers;
    std::vector<uint8_t> readback_scratch;
    std::vector<DownscaleTarget> downscale_targets;

    // Does the driver expose GL_ARB_parallel_shader_compile (or its KHR variant)?
    bool support_parallel_shader_compile = false;
    // What the user asked for. Kept separate because set_async_compilation may be
    // called before the extension has been probed.
    bool async_compilation_requested = false;
    // Translate shaders on worker threads and drop draws whose program is not ready.
    // Does not need any extension.
    bool use_async_compilation = false;
    // Skip the GL status queries that would block the render thread. Only possible
    // with GL_ARB_parallel_shader_compile.
    bool defer_gl_status_checks = false;

    // Programs whose linking is in flight, polled from the render thread.
    std::vector<PendingProgram> pending_programs;

    // gxp -> GLSL/SPIR-V translations in flight. Guarded by shader_translations_mutex.
    // Entries are shared so that a worker can safely finish writing to one that has
    // already been dropped from the map.
    std::mutex shader_translations_mutex;
    std::map<Sha256Hash, std::shared_ptr<ShaderTranslation>> shader_translations;

    std::unique_ptr<CompileWorker> compile_worker;
    bool compile_worker_unavailable = false;
    // Programs being linked by compile_worker, by job id
    std::map<uint64_t, WorkerProgram> worker_programs;
    uint64_t next_worker_job = 0;
    // Translated sources of the fragment [0] and vertex [1] shaders, to link new pairs on the worker
    std::map<Sha256Hash, std::shared_ptr<const ShaderTranslation>> shader_sources[2];

    std::vector<std::thread> shader_translate_threads;
    std::mutex shader_translate_queue_mutex;
    std::condition_variable shader_translate_queue_cond;
    std::deque<ShaderTranslateRequest *> shader_translate_queue;
    bool shader_translate_abort = false;

    bool init() override;
    void cleanup() override;
    void late_init(const Config &cfg, const std::string_view game_id, MemState &mem) override;

    TextureCache *get_texture_cache() override {
        return &texture_cache;
    }

    void render_frame(DisplayState &display, const GxmState &gxm, MemState &mem) override;
    void swap_window() override;
    bool set_current() override;
    void done_current() override;
    std::vector<uint32_t> dump_frame(DisplayState &display, uint32_t &width, uint32_t &height) override;

    int get_supported_filters() override;
    void set_screen_filter(const std::string_view &filter) override;
    int get_max_anisotropic_filtering() override;
    void set_anisotropic_filtering(int anisotropic_filtering) override;
    int get_max_2d_texture_width() override;
    void set_async_compilation(bool enable) override;

    // Move every pending program that the driver has finished linking to its final state.
    // Must be called from the render thread. Cheap when nothing is pending.
    void poll_pending_programs();
    // Created on first use, as it needs the render context to be current. Null if not supported.
    CompileWorker *get_compile_worker();

    void start_shader_translate_threads();
    void stop_shader_translate_threads();
    // Entry point of each shader translation worker.
    void shader_translate_thread();
    // Hand a request over to the workers. Takes ownership of the request.
    void enqueue_shader_translation(ShaderTranslateRequest *request);

    std::string_view get_gpu_name() override;

    void precompile_shader(const ShadersHash &hash) override;
    void preclose_action() override;

    bool has_pending_readbacks() const override {
        return !pending_readbacks.empty();
    }
    void complete_readbacks(MemState &mem, bool wait) override;
    void complete_readbacks_overlapping(MemState &mem, Address begin, Address end) override;
    GLuint get_downscale_framebuffer(uint32_t width, uint32_t height, GLenum internal_format);
};

} // namespace renderer::gl
