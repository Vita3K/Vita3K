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

#include <glutil/shader.h>

#include <glutil/gl.h>
#include <glutil/object.h>

#include <util/log.h>

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

/**
 * \brief Loads (compiles, links) GLSL shaders at specified file paths
 * \param vertex_file_path File path of the vertex shader
 * \param fragment_file_path File path of the fragment shader
 * \return SharedGLObject that holds the resulting program id. Empty if loading was unsuccessful
 */
UniqueGLObject gl::load_shaders(const std::string &vertex_file_path, const std::string &fragment_file_path) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

    // Read the vertex/fragment shader code from files
    std::string vs_code;
    std::ifstream vs_stream(vertex_file_path, std::ios::in);
    if (vs_stream.is_open()) {
        std::stringstream sstr;
        sstr << vs_stream.rdbuf();
        vs_code = sstr.str();
        vs_stream.close();
    } else {
        LOG_ERROR("Couldn't open shader: {}", vertex_file_path);
        return UniqueGLObject();
    }

    std::string fs_code;
    std::ifstream fs_stream(fragment_file_path, std::ios::in);
    if (fs_stream.is_open()) {
        std::stringstream sstr;
        sstr << fs_stream.rdbuf();
        fs_code = sstr.str();
        fs_stream.close();
    }

    GLint result = 0;
    int info_log_length;

    // Compile vertex shader
    char const *vs_source_pointer = vs_code.c_str();
    glShaderSource(vs, 1, &vs_source_pointer, NULL);
    glCompileShader(vs);

    // Check vertex shader
    glGetShaderiv(vs, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> vertex_shader_error_message(info_log_length + 1);
        glGetShaderInfoLog(vs, info_log_length, NULL, &vertex_shader_error_message[0]);
        LOG_ERROR("Error compiling vertex shader: {}\n", &vertex_shader_error_message[0]);
        return UniqueGLObject();
    }

    // Compile fragment shader
    char const *fs_source_pointer = fs_code.c_str();
    glShaderSource(fs, 1, &fs_source_pointer, NULL);
    glCompileShader(fs);

    // Check fragment shader
    glGetShaderiv(fs, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> fragment_shader_error_message(info_log_length + 1);
        glGetShaderInfoLog(fs, info_log_length, NULL, &fragment_shader_error_message[0]);
        LOG_ERROR("Error compiling fragment shader: {}\n", &fragment_shader_error_message[0]);
        return UniqueGLObject();
    }

    // Link the program
    const auto program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Check the program
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> program_error_message(info_log_length + 1);
        glGetProgramInfoLog(program, info_log_length, NULL, &program_error_message[0]);
        LOG_ERROR("Error linking shader program: {}\n", &program_error_message[0]);
        return UniqueGLObject();
    }

    glDetachShader(program, vs);
    glDetachShader(program, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    UniqueGLObject program_ptr = std::make_unique<GLObject>();
    if (!program_ptr->init(program, &glDeleteProgram)) {
        return UniqueGLObject();
    }

    return program_ptr;
}
