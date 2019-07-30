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

#include <glutil/gl.h>

#include <array>
#include <cassert>

template <size_t Size>
class GLObjectArray {
public:
    typedef void Generator(GLsizei, GLuint *);
    typedef void Deleter(GLsizei, const GLuint *);

    GLObjectArray() {
        names.fill(0);
    }

    ~GLObjectArray() {
        assert(deleter != nullptr);
        deleter(static_cast<GLsizei>(names.size()), &names[0]);
        names.fill(0);
    }

    bool init(Generator *generator, Deleter *deleter) {
        assert(generator != nullptr);
        assert(deleter != nullptr);
        this->deleter = deleter;
        generator(static_cast<GLsizei>(names.size()), &names[0]);

        return glGetError() == GL_NO_ERROR;
    }

    const GLuint operator[](size_t i) const {
        assert(i >= 0);
        assert(i < names.size());
        assert(names[i] != 0);
        return names[i];
    }

    size_t size() const {
        return names.size();
    }

private:
    typedef std::array<GLuint, Size> Names;

    GLObjectArray(const GLObjectArray &);
    const GLObjectArray &operator=(const GLObjectArray &);

    Names names;
    Deleter *deleter = nullptr;
};
