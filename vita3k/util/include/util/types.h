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

#include <cstdint>

/* SCE types */
typedef int8_t SceChar8;
typedef uint8_t SceUChar8;

typedef int8_t SceInt8;
typedef uint8_t SceUInt8;

typedef int16_t SceShort16;
typedef uint16_t SceUShort16;

typedef int16_t SceInt16;
typedef uint16_t SceUInt16;

typedef int32_t SceInt32;
typedef uint32_t SceUInt32;

typedef int32_t SceInt;
typedef uint32_t SceUInt;

typedef int64_t SceInt64;
typedef uint64_t SceUInt64;

typedef int64_t SceLong64;
typedef uint64_t SceULong64;

typedef unsigned int SceSize;
typedef int SceSSize;

typedef int SceBool;

enum {
    SCE_FALSE,
    SCE_TRUE
};

typedef float SceFloat;
typedef float SceFloat32;

typedef double SceDouble;
typedef double SceDouble64;

typedef signed char SceSByte;
typedef signed char SceSByte8;

typedef unsigned char SceByte;
typedef unsigned char SceByte8;

typedef uint16_t SceWChar16;
typedef uint32_t SceWChar32;

typedef void SceVoid;
typedef void *ScePVoid;

typedef int SceIntPtr;
typedef unsigned int SceUIntPtr;
typedef SceUIntPtr SceUIntVAddr;

/* PSP2 Specific types */
typedef struct SceIVector2 {
    SceInt x;
    SceInt y;
} SceIVector2;

typedef struct SceFVector2 {
    SceFloat x;
    SceFloat y;
} SceFVector2;

typedef struct SceIVector3 {
    SceInt x;
    SceInt y;
    SceInt z;
} SceIVector3;

typedef struct SceFVector3 {
    SceFloat x;
    SceFloat y;
    SceFloat z;
} SceFVector3;

typedef struct SceIVector4 {
    SceInt x;
    SceInt y;
    SceInt z;
    SceInt w;
} SceIVector4;

typedef struct SceUVector4 {
    SceUInt x;
    SceUInt y;
    SceUInt z;
    SceUInt w;
} SceUVector4;

typedef struct SceFVector4 {
    SceFloat x;
    SceFloat y;
    SceFloat z;
    SceFloat w;
} SceFVector4;

typedef struct SceIMatrix2 {
    SceIVector2 x;
    SceIVector2 y;
} SceIMatrix2;

typedef struct SceFMatrix2 {
    SceFVector2 x;
    SceFVector2 y;
} SceFMatrix2;

typedef struct SceIMatrix3 {
    SceIVector3 x;
    SceIVector3 y;
    SceIVector3 z;
} SceIMatrix3;

typedef struct SceFMatrix3 {
    SceFVector3 x;
    SceFVector3 y;
    SceFVector3 z;
} SceFMatrix3;

typedef struct SceIMatrix4 {
    SceIVector4 x;
    SceIVector4 y;
    SceIVector4 z;
    SceIVector4 w;
} SceIMatrix4;

typedef struct SceUMatrix4 {
    SceUVector4 x;
    SceUVector4 y;
    SceUVector4 z;
    SceUVector4 w;
} SceUMatrix4;

typedef struct SceFMatrix4 {
    SceFVector4 x;
    SceFVector4 y;
    SceFVector4 z;
    SceFVector4 w;
} SceFMatrix4;

typedef struct SceFQuaternion {
    SceFloat x;
    SceFloat y;
    SceFloat z;
    SceFloat w;
} SceFQuaternion;

typedef struct SceFColor {
    SceFloat r;
    SceFloat g;
    SceFloat b;
    SceFloat a;
} SceFColor;

typedef struct SceFPlane {
    SceFloat a;
    SceFloat b;
    SceFloat c;
    SceFloat d;
} SceFPlane;

typedef union SceUnion32 {
    unsigned int ui;
    int i;
    unsigned short us[2];
    short s[2];
    unsigned char uc[4];
    char c[4];
    float f;
    void *p;
} SceUnion32;

typedef union SceUnion64 {
    SceULong64 ull;
    SceLong64 ll;
    unsigned int ui[2];
    int i[2];
    unsigned short us[4];
    short s[4];
    unsigned char uc[8];
    char c[8];
    float f[2];
    SceFVector2 fv;
    SceIVector2 iv;
} SceUnion64;

typedef union SceUnion128 {
    SceULong64 ull[2];
    SceLong64 ll[2];
    unsigned int ui[4];
    int i[4];
    unsigned short us[8];
    short s[8];
    unsigned char uc[16];
    char c[16];
    float f[4];
    SceFVector4 fv;
    SceFQuaternion fq;
    SceFPlane fp;
    SceFColor fc;
    SceIVector4 iv;
} SceUnion128;

typedef struct SceDateTime {
    unsigned short year;
    unsigned short month;
    unsigned short day;
    unsigned short hour;
    unsigned short minute;
    unsigned short second;
    unsigned int microsecond;
} SceDateTime;

typedef int SceMode;
typedef SceInt64 SceOff;

typedef int SceUID;

typedef int ScePID;
#define PSP2_KERNEL_PROCESS_ID_SELF 0 //!< Current running process ID is always 0

typedef char *SceName;
#define PSP2_UID_NAMELEN 31 //!< Maximum length for kernel object names
