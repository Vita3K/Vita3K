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

#include <glutil/object.h>

#include <cassert>

GLObject::~GLObject() {
    if (aggregate_deleter != nullptr) {
        aggregate_deleter(1, &name);
    }
    if (singular_deleter != nullptr) {
        singular_deleter(name);
    }
    name = 0;
}

bool GLObject::init(GLuint name) {
    assert(name != 0);
    assert(this->name == 0);
    this->name = name;

    return name != 0;
}

bool GLObject::init(GLuint name, AggregateDeleter aggregate_deleter) {
    if (aggregate_deleter)
        this->aggregate_deleter = aggregate_deleter;

    return init(name);
}

bool GLObject::init(GLuint name, SingularDeleter singular_deleter) {
    if (singular_deleter)
        this->singular_deleter = singular_deleter;

    return init(name);
}

GLuint GLObject::get() const {
    assert(name != 0);
    return name;
}

GLObject::operator GLuint() const {
    assert(name != 0);
    return name;
}

GLObject::operator bool() const {
    return name;
}
