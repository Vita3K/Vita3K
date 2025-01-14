// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <module/module.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(ScePafStdc);

EXPORT(int, sce_paf_aeabi_atexit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_cxa_atexit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_exit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_memalign) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_abs) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_atexit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_atof) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_atoi) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_atol) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_atoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_bcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_bcopy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_bsearch) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_bzero) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_exit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_fclose) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_fflush) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_fgetc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_fopen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_fputc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_fread) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_free) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_free2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_fseek) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_ftell) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_fwrite) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_longjmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_look_ctype_table) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_malloc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_malloc2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_memchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_memcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_memcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_memcpy2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_memmove) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_memset) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_memset2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_qsort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_rand) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_setjmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_snprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_sprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_srand) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strcasecmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strcat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strcspn) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strlcat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strlcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strlen, const char *str) {
    TRACY_FUNC(sce_paf_private_strlen, str);
    return strlen(str);
}

EXPORT(int, sce_paf_private_strncasecmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strncmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strncpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strpbrk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strspn) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strtof) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strtok) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strtok_r) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strtol) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strtoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strtoul) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_strtoull) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_swprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_tolower) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_toupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_towlower) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_vsnprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_vsprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcscasecmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcscat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcschr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcscmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcscpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcscspn) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcslen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcsncasecmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcsncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcsncmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcsncpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcsnlen) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcspbrk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcsrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wcsspn) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wmemchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wmemcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wmemcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wmemmove) {
    return UNIMPLEMENTED();
}

EXPORT(int, sce_paf_private_wmemset) {
    return UNIMPLEMENTED();
}
