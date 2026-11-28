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

#include <renderer/gl/compile_worker.h>

#include <util/log.h>

#include <algorithm>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>

#pragma comment(lib, "opengl32.lib")
#endif

namespace renderer::gl {

#ifdef _WIN32
namespace {
using CreateContextAttribsARB = HGLRC(WINAPI *)(HDC, HGLRC, const int *);

constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
constexpr wchar_t window_class[] = L"Vita3KCompileWorker";

struct ContextInfo {
    CreateContextAttribsARB create_context;
    int pixel_format;
    PIXELFORMATDESCRIPTOR pixel_format_desc;
    int attribs[7];
};

GLuint compile_shader(GLenum type, const ShaderTranslation &source, bool spirv) {
    const GLuint shader = glCreateShader(type);
    if (spirv) {
        glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, source.spirv.data(), static_cast<GLsizei>(source.spirv.size() * sizeof(uint32_t)));
        glSpecializeShaderARB(shader, type == GL_VERTEX_SHADER ? "main_vs" : "main_fs", 0, nullptr, nullptr);
    } else {
        const GLchar *text = source.glsl.c_str();
        const GLint length = static_cast<GLint>(source.glsl.size());
        glShaderSource(shader, 1, &text, &length);
        glCompileShader(shader);
    }
    return shader;
}
} // namespace

std::unique_ptr<CompileWorker> CompileWorker::create() {
    const HDC dc = wglGetCurrentDC();
    ContextInfo info{};
    info.create_context = reinterpret_cast<CreateContextAttribsARB>(wglGetProcAddress("wglCreateContextAttribsARB"));
    info.pixel_format = dc ? GetPixelFormat(dc) : 0;
    if (!info.create_context || !info.pixel_format)
        return nullptr;
    DescribePixelFormat(dc, info.pixel_format, sizeof(info.pixel_format_desc), &info.pixel_format_desc);

    GLint major = 0, minor = 0, profile = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    const int attribs[] = { WGL_CONTEXT_MAJOR_VERSION_ARB, major, WGL_CONTEXT_MINOR_VERSION_ARB, minor, WGL_CONTEXT_PROFILE_MASK_ARB, profile, 0 };
    std::copy(std::begin(attribs), std::end(attribs), info.attribs);

    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = window_class;
    RegisterClassW(&wc);

    // two threads are enough to keep up with a scene change, more only compete with the game
    constexpr int thread_count = 2;
    std::unique_ptr<CompileWorker> worker(new CompileWorker());
    for (int i = 0; i < thread_count; i++) {
        std::promise<bool> started;
        std::future<bool> result = started.get_future();
        try {
            worker->threads.emplace_back(&CompileWorker::run, worker.get(), &info, std::move(started));
        } catch (const std::system_error &) {
            LOG_WARN("Could not start a thread to compile shaders on, compiling them on the render thread");
            return nullptr;
        }
        if (!result.get()) {
            LOG_WARN("Could not create a context to compile shaders on, compiling them on the render thread");
            return nullptr;
        }
    }

    LOG_INFO("Linking shaders on {} threads", thread_count);
    return worker;
}

void CompileWorker::run(void *context_info, std::promise<bool> started) {
    const ContextInfo info = *static_cast<const ContextInfo *>(context_info);

    // message-only, so it does not take part in broadcasts it would never answer
    const HWND window = CreateWindowExW(0, window_class, L"", 0, 0, 0, 1, 1, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    const HDC dc = window ? GetDC(window) : nullptr;
    HGLRC context = nullptr;
    if (dc && SetPixelFormat(dc, info.pixel_format, &info.pixel_format_desc))
        context = info.create_context(dc, nullptr, info.attribs);
    if (!context || !wglMakeCurrent(dc, context)) {
        LOG_ERROR("Failed to create a shader compilation context: {}", GetLastError());
        if (context)
            wglDeleteContext(context);
        if (dc)
            ReleaseDC(window, dc);
        if (window)
            DestroyWindow(window);
        started.set_value(false);
        return;
    }
    started.set_value(true);

    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cond.wait(lock, [this] { return abort || !jobs.empty(); });
            if (abort)
                break;
            job = std::move(jobs.front());
            jobs.pop_front();
        }

        const GLuint frag = compile_shader(GL_FRAGMENT_SHADER, *job.frag, job.spirv);
        const GLuint vert = compile_shader(GL_VERTEX_SHADER, *job.vert, job.spirv);
        const GLuint program = glCreateProgram();
        glAttachShader(program, frag);
        glAttachShader(program, vert);
        glLinkProgram(program);
        // waits for the link, errors are reported when the render thread links it again
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        glDeleteProgram(program);
        glDeleteShader(frag);
        glDeleteShader(vert);

        std::lock_guard<std::mutex> lock(mutex);
        done.push_back(job.id);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    ReleaseDC(window, dc);
    DestroyWindow(window);
}

CompileWorker::~CompileWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        abort = true;
    }
    cond.notify_all();
    for (std::thread &thread : threads)
        thread.join();
}

void CompileWorker::submit(Job job) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        jobs.push_back(std::move(job));
    }
    cond.notify_one();
}

std::vector<uint64_t> CompileWorker::take_done() {
    std::lock_guard<std::mutex> lock(mutex);
    return std::exchange(done, {});
}
#else
std::unique_ptr<CompileWorker> CompileWorker::create() {
    return nullptr;
}

CompileWorker::~CompileWorker() = default;

void CompileWorker::submit(Job job) {}

std::vector<uint64_t> CompileWorker::take_done() {
    return {};
}

void CompileWorker::run(void *context_info, std::promise<bool> started) {}
#endif

} // namespace renderer::gl
