// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <glutil/gl.h>

#include <functional>
#include <memory>

class GLObject {
public:
    using SingularDeleter = std::function<void(GLuint)>;
    using AggregateDeleter = std::function<void(int, const GLuint *)>;

    GLObject() = default;
    ~GLObject();

    bool init(GLuint name, AggregateDeleter aggregate_deleter);
    bool init(GLuint name, SingularDeleter singular_deleter);
    GLuint get() const;
    operator GLuint() const;
    operator bool() const;

private:
    bool init(GLuint name);
    const GLObject &operator=(const GLObject &) = delete;

    GLuint name = 0;
    AggregateDeleter aggregate_deleter = nullptr;
    SingularDeleter singular_deleter = nullptr;
};

using SharedGLObject = std::shared_ptr<GLObject>;
using UniqueGLObject = std::unique_ptr<GLObject>;
