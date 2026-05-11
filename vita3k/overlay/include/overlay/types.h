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
// Copyright RPCS3
// SPDX-License-Identifier: GPL-2.0
// Code heavily referenced/taken from https://github.com/RPCS3/rpcs3/tree/master/rpcs3/Emu/RSX/Overlays

#pragma once

#include <cstdint>

namespace overlay {

struct vertex {
    float values[4];

    vertex() = default;

    vertex(float x, float y) {
        vec2(x, y);
    }

    vertex(float x, float y, float z) {
        vec3(x, y, z);
    }

    vertex(float x, float y, float z, float w) {
        vec4(x, y, z, w);
    }

    vertex(int x, int y, int z, int w) {
        vec4(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w));
    }

    float &operator[](int index) {
        return values[index];
    }

    float operator[](int index) const {
        return values[index];
    }

    float &x() { return values[0]; }
    float &y() { return values[1]; }
    float &z() { return values[2]; }
    float &w() { return values[3]; }

    float x() const { return values[0]; }
    float y() const { return values[1]; }
    float z() const { return values[2]; }
    float w() const { return values[3]; }

    void vec2(float x, float y) {
        values[0] = x;
        values[1] = y;
        values[2] = 0.f;
        values[3] = 1.f;
    }

    void vec3(float x, float y, float z) {
        values[0] = x;
        values[1] = y;
        values[2] = z;
        values[3] = 1.f;
    }

    void vec4(float x, float y, float z, float w) {
        values[0] = x;
        values[1] = y;
        values[2] = z;
        values[3] = w;
    }

    void operator+=(const vertex &other) {
        values[0] += other.values[0];
        values[1] += other.values[1];
        values[2] += other.values[2];
        values[3] += other.values[3];
    }

    void operator-=(const vertex &other) {
        values[0] -= other.values[0];
        values[1] -= other.values[1];
        values[2] -= other.values[2];
        values[3] -= other.values[3];
    }

    void operator/=(const vertex &other) {
        values[0] /= other.values[0];
        values[1] /= other.values[1];
        values[2] /= other.values[2];
        values[3] /= other.values[3];
    }

    void operator/=(float rhs) {
        values[0] /= rhs;
        values[1] /= rhs;
        values[2] /= rhs;
        values[3] /= rhs;
    }

    void operator*=(const vertex &other) {
        values[0] *= other.values[0];
        values[1] *= other.values[1];
        values[2] *= other.values[2];
        values[3] *= other.values[3];
    }

    void operator*=(float rhs) {
        values[0] *= rhs;
        values[1] *= rhs;
        values[2] *= rhs;
        values[3] *= rhs;
    }

    vertex operator+(const vertex &other) const {
        vertex result = *this;
        result += other;
        return result;
    }

    vertex operator-(const vertex &other) const {
        vertex result = *this;
        result -= other;
        return result;
    }
};

struct color4f {
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
    float a = 0.f;

    bool operator==(const color4f &) const = default;

    color4f operator*(const color4f &other) const {
        return { r * other.r, g * other.g, b * other.b, a * other.a };
    }

    void operator*=(const color4f &other) {
        r *= other.r;
        g *= other.g;
        b *= other.b;
        a *= other.a;
    }

    color4f operator*(float s) const {
        return { r * s, g * s, b * s, a * s };
    }

    void operator*=(float s) {
        r *= s;
        g *= s;
        b *= s;
        a *= s;
    }

    color4f operator+(const color4f &other) const {
        return { r + other.r, g + other.g, b + other.b, a + other.a };
    }

    void operator+=(const color4f &other) {
        r += other.r;
        g += other.g;
        b += other.b;
        a += other.a;
    }
};

struct vector3f {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;

    bool operator==(const vector3f &) const = default;

    vector3f operator*(float s) const {
        return { x * s, y * s, z * s };
    }

    vector3f operator+(const vector3f &other) const {
        return { x + other.x, y + other.y, z + other.z };
    }
};

struct areaf {
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;

    float width() const { return x2 - x1; }
    float height() const { return y2 - y1; }

    bool operator==(const areaf &) const = default;

    // Convenience: build from integer position + size
    static areaf from_rect(int16_t x, int16_t y, uint16_t w, uint16_t h) {
        return { static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(x + w), static_cast<float>(y + h) };
    }
};

struct sizef {
    float width = 0.f;
    float height = 0.f;

    bool operator==(const sizef &) const = default;
};

struct positioni {
    int32_t x = 0;
    int32_t y = 0;

    bool operator==(const positioni &) const = default;
};

struct size2i {
    int32_t width = 0;
    int32_t height = 0;

    bool operator==(const size2i &) const = default;
};

struct size3u {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;

    bool operator==(const size3u &) const = default;
};

} // namespace overlay
