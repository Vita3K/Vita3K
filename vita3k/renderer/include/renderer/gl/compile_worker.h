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

#include <renderer/gl/types.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace renderer::gl {

// Links programs on threads that have their own GL context, sharing nothing with the render
// context. The programs are thrown away: the point is to fill the driver shader cache, so that
// linking the same sources on the render thread afterwards is nearly free.
// Some drivers (AMD) advertise GL_ARB_parallel_shader_compile but still link on the thread that
// queries the result, and a shared context would serialize with the render context.
class CompileWorker {
public:
    struct Job {
        uint64_t id;
        std::shared_ptr<const ShaderTranslation> frag;
        std::shared_ptr<const ShaderTranslation> vert;
        bool spirv;
    };

    // Must be called with the render context current. Returns null where this is not supported.
    static std::unique_ptr<CompileWorker> create();
    ~CompileWorker();

    void submit(Job job);
    // Ids of the jobs done since the last call
    std::vector<uint64_t> take_done();

private:
    CompileWorker() = default;
    void run(void *context_info, std::promise<bool> started);

    std::mutex mutex;
    std::condition_variable cond;
    std::deque<Job> jobs;
    std::vector<uint64_t> done;
    std::vector<std::thread> threads;
    bool abort = false;
};

} // namespace renderer::gl
