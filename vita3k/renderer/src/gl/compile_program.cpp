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

#include <renderer/profile.h>
#include <renderer/shaders.h>
#include <renderer/types.h>

#include <renderer/gl/state.h>
#include <renderer/gl/types.h>

#include <util/log.h>

#include <shader/spirv_recompiler.h>

#include <iomanip>
#include <vector>

namespace renderer::gl {
// Report the compilation log of a shader. This blocks until the driver is done with it,
// so it must only be called once the owning program is known to have finished linking.
static void log_shader_error(GLuint shader, const char *type_str) {
    GLint is_compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled != GL_FALSE)
        return;

    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 1) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());

        LOG_ERROR("Failed to compile {} shader: {}", type_str, log.data());
    } else {
        LOG_ERROR("Failed to compile {} shader", type_str);
    }
}

// When defer_check is set, no status query is performed. Every query other than
// GL_COMPLETION_STATUS_ARB blocks until the driver is done, which is exactly what
// asynchronous compilation is trying to avoid. Errors are then surfaced at link time.
static SharedGLObject compile_glsl(GLenum type, const std::string &source, bool defer_check) {
    R_PROFILE(__func__);

    SharedGLObject shader = std::make_shared<GLObject>();
    if (!shader->init(glCreateShader(type), glDeleteShader)) {
        return SharedGLObject();
    }

    const GLchar *source_glchar = source.c_str();
    const GLint length = static_cast<GLint>(source.length());
    glShaderSource(shader->get(), 1, &source_glchar, &length);

    glCompileShader(shader->get());

    if (defer_check) {
        return shader;
    }

    GLint log_length = 0;
    glGetShaderiv(shader->get(), GL_INFO_LOG_LENGTH, &log_length);

    // Intel driver returns an info log length of at least 1 even if it is empty.
    if (log_length > 1) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetShaderInfoLog(shader->get(), log_length, nullptr, log.data());

        LOG_ERROR("{}", log.data());
    }

    GLint is_compiled = GL_FALSE;
    glGetShaderiv(shader->get(), GL_COMPILE_STATUS, &is_compiled);
    assert(is_compiled != GL_FALSE);
    if (!is_compiled) {
        return SharedGLObject();
    }

    return shader;
}

static SharedGLObject compile_spirv(GLenum type, const std::vector<std::uint32_t> &source, bool defer_check) {
    R_PROFILE(__func__);

    SharedGLObject shader = std::make_shared<GLObject>();
    if (!shader->init(glCreateShader(type), glDeleteShader)) {
        return SharedGLObject();
    }

    const GLchar *source_glchar = reinterpret_cast<const GLchar *>(source.data());
    const GLint length = static_cast<GLint>(source.size() * sizeof(std::uint32_t));

    GLuint need_compile[1] = { shader->get() };
    const char *shader_entry = (type == GL_VERTEX_SHADER) ? "main_vs" : "main_fs";

    glShaderBinary(1, need_compile, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, source_glchar, length);
    glSpecializeShaderARB(need_compile[0], shader_entry, 0, nullptr, nullptr);

    if (defer_check) {
        return shader;
    }

    GLint log_length = 0;
    glGetShaderiv(shader->get(), GL_INFO_LOG_LENGTH, &log_length);

    // Intel driver returns an info log length of at least 1 even if it is empty.
    if (log_length > 1) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetShaderInfoLog(shader->get(), log_length, nullptr, log.data());

        LOG_ERROR("{}", log.data());
    }

    GLint is_compiled = GL_FALSE;
    glGetShaderiv(shader->get(), GL_COMPILE_STATUS, &is_compiled);
    assert(is_compiled != GL_FALSE);
    if (!is_compiled) {
        return SharedGLObject();
    }

    return shader;
}

static std::string convert_hash_to_hex(const Sha256Hash &hash) {
    std::string str;
    str.reserve(hash.size() * 2);

    for (size_t i = 0; i < hash.size(); ++i) {
        fmt::format_to(std::back_inserter(str), "{:02x}", hash[i]);
    }

    return str;
}

// Issue the link without waiting for it. No status query is performed here.
static SharedGLObject link_program(const SharedGLObject &frag_shader, const SharedGLObject &vert_shader) {
    SharedGLObject program = std::make_shared<GLObject>();
    if (!program->init(glCreateProgram(), glDeleteProgram)) {
        return SharedGLObject();
    }

    glAttachShader(program->get(), frag_shader->get());
    glAttachShader(program->get(), vert_shader->get());
    glLinkProgram(program->get());

    return program;
}

// Collect the result of a link that is known to be complete. Blocking queries are fine here.
static bool finalize_program(const PendingProgram &pending) {
    const GLuint program = pending.program->get();

    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    // Intel driver returns an info log length of at least 1 even if it is empty.
    if (log_length > 1) {
        std::vector<GLchar> log;
        log.resize(log_length);
        glGetProgramInfoLog(program, log_length, nullptr, log.data());

        LOG_ERROR("{}\n", log.data());
    }

    GLint is_linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);

    if (is_linked == GL_FALSE) {
        // The shader status checks were skipped when the compilation was issued,
        // so this is our first chance to tell whether a shader is at fault.
        log_shader_error(pending.frag_shader->get(), "fragment");
        log_shader_error(pending.vert_shader->get(), "vertex");
    }

    glDetachShader(program, pending.frag_shader->get());
    glDetachShader(program, pending.vert_shader->get());

    return is_linked != GL_FALSE;
}

// Link and wait for the result, for the paths where blocking is acceptable.
static SharedGLObject link_program_sync(ProgramCache &program_cache, const SharedGLObject &frag_shader, const SharedGLObject &vert_shader, const ProgramHashes &hashes) {
    SharedGLObject program = link_program(frag_shader, vert_shader);
    if (!program) {
        return SharedGLObject();
    }

    const PendingProgram pending{ hashes, program, frag_shader, vert_shader };
    if (!finalize_program(pending)) {
        program_cache[hashes] = { SharedGLObject(), ProgramStatus::Failed };
        return SharedGLObject();
    }

    program_cache[hashes] = { program, ProgramStatus::Ready };

    return program;
}

void GLState::poll_pending_programs() {
    if (pending_programs.empty())
        return;

    for (auto it = pending_programs.begin(); it != pending_programs.end();) {
        GLint completed = GL_FALSE;
        glGetProgramiv(it->program->get(), GL_COMPLETION_STATUS_ARB, &completed);

        if (completed == GL_FALSE) {
            ++it;
            continue;
        }

        if (finalize_program(*it)) {
            program_cache[it->hashes] = { it->program, ProgramStatus::Ready };
        } else {
            LOG_CRITICAL("Failed to link program asynchronously");
            // Keep the entry around so that we do not retry this program every single draw.
            program_cache[it->hashes] = { SharedGLObject(), ProgramStatus::Failed };
        }

        it = pending_programs.erase(it);
    }
}

void GLState::shader_translate_thread() {
    while (true) {
        ShaderTranslateRequest *request = nullptr;

        {
            std::unique_lock<std::mutex> lock(shader_translate_queue_mutex);
            shader_translate_queue_cond.wait(lock, [this] {
                return shader_translate_abort || !shader_translate_queue.empty();
            });

            if (shader_translate_abort)
                return;

            request = shader_translate_queue.front();
            shader_translate_queue.pop_front();
        }

        // Nothing here touches GL: this is the gxp -> GLSL/SPIR-V recompilation only,
        // which is also what the vulkan backend already runs on its worker threads.
        bool ok = false;
        if (request->spirv) {
            request->target->spirv = load_spirv_shader(*request->program, features, false, request->hints, request->maskupdate,
                shaders_path, shaders_log_path, shader_version + "spv", request->use_shader_cache);
            ok = !request->target->spirv.empty();
        } else {
            request->target->glsl = load_glsl_shader(*request->program, features, request->hints, request->maskupdate,
                shaders_path, shaders_log_path, shader_version, request->use_shader_cache);
            ok = !request->target->glsl.empty();
        }

        // Publishes the generated source to the render thread.
        request->target->status.store(ok ? ShaderStatus::Ready : ShaderStatus::Failed, std::memory_order_release);

        request->refcount->fetch_sub(1, std::memory_order_release);
        delete request;
    }
}

// Asynchronous counterpart of get_or_compile_shader. The expensive recompilation is handed
// to a worker thread, and the GL object is only created here, on the render thread, once the
// generated source is available.
static SharedGLObject get_or_translate_shader(GLState &renderer, const SceGxmProgram *program, std::atomic<uint32_t> *refcount,
    const Sha256Hash &hash, ShaderCache &cache, const GLenum type, const shader::Hints &hints,
    bool shader_cache, bool spirv, bool maskupdate, bool &is_translating) {
    is_translating = false;

    const auto cached = cache.find(hash);
    if (cached != cache.end()) {
        // A null entry means we already tried and failed.
        return cached->second;
    }

    const bool use_spirv = renderer.features.spirv_shader && spirv;

    std::shared_ptr<ShaderTranslation> entry;
    {
        // Lock ordering is always shader_translations_mutex then shader_translate_queue_mutex.
        std::lock_guard<std::mutex> guard(renderer.shader_translations_mutex);

        const auto it = renderer.shader_translations.find(hash);
        if (it == renderer.shader_translations.end()) {
            entry = std::make_shared<ShaderTranslation>();
            renderer.shader_translations.emplace(hash, entry);

            auto *request = new ShaderTranslateRequest();
            request->target = entry;
            request->program = program;
            request->refcount = refcount;
            request->is_vertex = (type == GL_VERTEX_SHADER);
            request->maskupdate = maskupdate;
            request->spirv = use_spirv;
            request->use_shader_cache = shader_cache;
            request->hints = hints;
            if (hints.attributes)
                request->attributes = *hints.attributes;
            request->hints.attributes = &request->attributes;

            // Tell SceGxm that it must not destroy the program under our feet.
            refcount->fetch_add(1, std::memory_order_relaxed);
            renderer.enqueue_shader_translation(request);

            is_translating = true;
            return SharedGLObject();
        }

        entry = it->second;
    }

    const ShaderStatus status = entry->status.load(std::memory_order_acquire);
    if (status == ShaderStatus::Translating) {
        is_translating = true;
        return SharedGLObject();
    }

    SharedGLObject obj;
    if (status == ShaderStatus::Ready) {
        obj = use_spirv ? compile_spirv(type, entry->spirv, renderer.defer_gl_status_checks)
                        : compile_glsl(type, entry->glsl, renderer.defer_gl_status_checks);
        renderer.shaders_count_compiled++;
    } else {
        LOG_CRITICAL("Failed to translate shader:\n{}", hex_string(hash));
    }

    cache.emplace(hash, obj);

    {
        std::lock_guard<std::mutex> guard(renderer.shader_translations_mutex);
        renderer.shader_translations.erase(hash);
    }

    return obj;
}

static SharedGLObject compile_shader(const fs::path &shader_cache_path, const std::string &shader_version, const std::string &hash_hex,
    const char *type_str, const GLenum type, ShaderCache &cache, const Sha256Hash &hash) {
    // Set Shader version with hash

    // Load Shader
    const auto shader_name = shader_cache_path / fmt::format("{}-{}.{}", shader_version, hash_hex, type_str);
    const std::string shader = pre_load_shader_glsl(shader_name);
    if (shader.empty()) {
        LOG_WARN("{} shader is empty or not found:\n{}", type_str, hash_hex);
        return SharedGLObject();
    }

    // Compile Shader
    SharedGLObject obj = compile_glsl(type, shader, false);
    if (!obj) {
        LOG_CRITICAL("Error in compile {} shader:\n{}", type_str, hash_hex);
        return SharedGLObject();
    }

    // Push shader Compiled
    cache.emplace(hash, obj);
    LOG_INFO("{} shader compiled: {}", type_str, hash_hex);

    return obj;
}

static std::vector<ShadersHash>::iterator get_shaders_hash_index(std::vector<ShadersHash> &shaders_cache_hashs, const Sha256Hash &frag_hash, const Sha256Hash &vert_hash) {
    const auto shader_hash_index = std::ranges::find_if(shaders_cache_hashs, [&](const ShadersHash &h) {
        return (h.frag == frag_hash) && (h.vert == vert_hash);
    });

    return shader_hash_index;
}

void pre_compile_program(GLState &renderer, const ShadersHash &hash) {
    if (fs::exists(renderer.shaders_path) && !fs::is_empty(renderer.shaders_path)) {
        // Compile Fragment Shader
        const auto frag_hash_hex = convert_hash_to_hex(hash.frag);
        const SharedGLObject frag_shader = compile_shader(renderer.shaders_path, renderer.shader_version,
            frag_hash_hex, "frag", GL_FRAGMENT_SHADER, renderer.fragment_shader_cache, hash.frag);
        if (!frag_shader) {
            return;
        }

        // Compile Vertex Shader
        const auto vert_hash_hex = convert_hash_to_hex(hash.vert);
        const SharedGLObject vert_shader = compile_shader(renderer.shaders_path, renderer.shader_version,
            vert_hash_hex, "vert", GL_VERTEX_SHADER, renderer.vertex_shader_cache, hash.vert);
        if (!vert_shader) {
            return;
        }

        // Compile Program
        // Kept synchronous: this runs behind the precompilation overlay, where blocking is
        // expected and the reported progress has to match what is actually done.
        const ProgramHashes hashes(hash.frag, hash.vert);
        link_program_sync(renderer.program_cache, frag_shader, vert_shader, hashes);
        renderer.programs_count_pre_compiled++;
        LOG_INFO("Program Compiled {}/{}", renderer.programs_count_pre_compiled, renderer.shaders_cache_hashs.size());
    }
}

static SharedGLObject get_or_compile_shader(const SceGxmProgram *program, const FeatureState &features, const Sha256Hash &hash,
    ShaderCache &cache, const GLenum type, const shader::Hints &hints, bool shader_cache, bool spirv, bool maskupdate, const fs::path &shader_cache_path, const fs::path &shader_log_path, const std::string &shader_version, uint32_t &shaders_count_compiled, bool defer_check) {
    const auto cached = cache.find(hash);
    if (cached == cache.end()) {
        SharedGLObject obj = nullptr;

        // Need to compile new one and add it to cache
        if (features.spirv_shader && spirv) {
            obj = compile_spirv(type, load_spirv_shader(*program, features, false, hints, maskupdate, shader_cache_path, shader_log_path, shader_version + "spv", shader_cache), defer_check);
        } else {
            obj = compile_glsl(type, load_glsl_shader(*program, features, hints, maskupdate, shader_cache_path, shader_log_path, shader_version, shader_cache), defer_check);
        }

        cache.emplace(hash, obj);

        shaders_count_compiled++;

        return obj;
    }

    return cached->second;
}

SharedGLObject compile_program(GLState &renderer, GLContext &context, const GxmRecordState &state, const FeatureState &features, const MemState &mem,
    bool shader_cache, bool spirv, bool maskupdate, bool *is_compiling) {
    R_PROFILE(__func__);

    if (is_compiling)
        *is_compiling = false;

    assert(state.fragment_program);
    assert(state.vertex_program);

    // Non-const because the asynchronous path takes a reference on compile_threads_on,
    // which SceGxm waits on before destroying a program. Same as the vulkan backend.
    SceGxmVertexProgram &vertex_program_gxm = *state.vertex_program.get(mem);
    SceGxmFragmentProgram &fragment_program_gxm = *state.fragment_program.get(mem);

    const GLFragmentProgram &fragment_program = *reinterpret_cast<GLFragmentProgram *>(
        fragment_program_gxm.renderer_data.get());

    const GLVertexProgram &vertex_program = *reinterpret_cast<GLVertexProgram *>(
        vertex_program_gxm.renderer_data.get());

    const ProgramHashes hashes(fragment_program.hash, vertex_program.hash);

    // First pass, trying to find the program, since link is costly
    const ProgramCache::const_iterator cached = renderer.program_cache.find(hashes);
    if (cached != renderer.program_cache.end()) {
        if (cached->second.status == ProgramStatus::Compiling && is_compiling)
            *is_compiling = true;

        return cached->second.status == ProgramStatus::Ready ? cached->second.program : SharedGLObject();
    }

    // No... It doesn't exist. Now we try to find each object. If it doesn't exist then we can kind
    // of compile it again.

    // update the hints
    context.shader_hints.color_format = state.color_surface.colorFormat;
    context.shader_hints.attributes = &vertex_program_gxm.attributes;

    const bool defer_check = renderer.defer_gl_status_checks;

    SharedGLObject fragment_shader;
    SharedGLObject vertex_shader;

    if (renderer.use_async_compilation) {
        // Both are requested before bailing out, so that they are translated in parallel
        // instead of one draw apart.
        bool frag_translating = false;
        bool vert_translating = false;

        fragment_shader = get_or_translate_shader(renderer, fragment_program_gxm.program.get(mem), &fragment_program_gxm.compile_threads_on,
            fragment_program.hash, renderer.fragment_shader_cache, GL_FRAGMENT_SHADER, context.shader_hints,
            shader_cache, spirv, maskupdate, frag_translating);

        vertex_shader = get_or_translate_shader(renderer, vertex_program_gxm.program.get(mem), &vertex_program_gxm.compile_threads_on,
            vertex_program.hash, renderer.vertex_shader_cache, GL_VERTEX_SHADER, context.shader_hints,
            shader_cache, spirv, maskupdate, vert_translating);

        if (frag_translating || vert_translating) {
            // Deliberately not cached as Compiling: the next draw has to run the lookup above
            // again to notice that a worker is done.
            if (is_compiling)
                *is_compiling = true;

            return SharedGLObject();
        }
    } else {
        fragment_shader = get_or_compile_shader(fragment_program_gxm.program.get(mem), features, fragment_program.hash, renderer.fragment_shader_cache,
            GL_FRAGMENT_SHADER, context.shader_hints, shader_cache, spirv, maskupdate, renderer.shaders_path, renderer.shaders_log_path, renderer.shader_version, renderer.shaders_count_compiled, defer_check);

        vertex_shader = get_or_compile_shader(vertex_program_gxm.program.get(mem), features, vertex_program.hash, renderer.vertex_shader_cache,
            GL_VERTEX_SHADER, context.shader_hints, shader_cache, spirv, maskupdate, renderer.shaders_path, renderer.shaders_log_path, renderer.shader_version, renderer.shaders_count_compiled, defer_check);
    }

    if (!fragment_shader) {
        LOG_CRITICAL("Error in get/compile fragment vertex shader:\n{}", hex_string(fragment_program.hash));
        renderer.program_cache[hashes] = { SharedGLObject(), ProgramStatus::Failed };
        return SharedGLObject();
    }

    if (!vertex_shader) {
        LOG_CRITICAL("Error in get/compiled vertex shader:\n{}", hex_string(vertex_program.hash));
        renderer.program_cache[hashes] = { SharedGLObject(), ProgramStatus::Failed };
        return SharedGLObject();
    }

    // Save shader cache haches
    const auto shader_cache_hash_index = get_shaders_hash_index(renderer.shaders_cache_hashs, fragment_program.hash, vertex_program.hash);
    if (shader_cache_hash_index == renderer.shaders_cache_hashs.end()) {
        renderer.shaders_cache_hashs.push_back({ fragment_program.hash, vertex_program.hash });
        save_shaders_cache_hashs(renderer, renderer.shaders_cache_hashs);
    }

    if (!renderer.defer_gl_status_checks) {
        // Without GL_ARB_parallel_shader_compile there is no way to poll the link, so it has
        // to be waited on. The translation was still done off the render thread.
        return link_program_sync(renderer.program_cache, fragment_shader, vertex_shader, hashes);
    }

    SharedGLObject program = link_program(fragment_shader, vertex_shader);
    if (!program) {
        renderer.program_cache[hashes] = { SharedGLObject(), ProgramStatus::Failed };
        return SharedGLObject();
    }

    // Mark it as in flight and let the render thread pick the result up later.
    // The shaders are kept alive by the pending entry until then.
    renderer.program_cache[hashes] = { program, ProgramStatus::Compiling };
    renderer.pending_programs.push_back({ hashes, program, fragment_shader, vertex_shader });

    if (is_compiling)
        *is_compiling = true;

    return SharedGLObject();
}
} // namespace renderer::gl
