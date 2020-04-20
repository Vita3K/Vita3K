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

#include "SceLibc.h"
#include <io/functions.h>
#include <util/log.h>

Ptr<void> g_dso;

EXPORT(int, _Assert) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Btowc) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Ctype) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Dbl) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Denorm) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Exit) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FCbuild) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FDenorm) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FInf) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FNan) {
    return UNIMPLEMENTED();
}

EXPORT(int, _FSnan) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Files) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Flt) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Fltrounds) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Inf) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Iswctype) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LDenorm) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LInf) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LNan) {
    return UNIMPLEMENTED();
}

EXPORT(int, _LSnan) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Ldbl) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Lockfilelock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Locksyslock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Mbtowc) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Nan) {
    return UNIMPLEMENTED();
}

EXPORT(int, _PJP_C_Copyright) {
    return UNIMPLEMENTED();
}

EXPORT(int, _SCE_Assert) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Snan) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stderr) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stdin) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stdout) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stod) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stodx) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stof) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stofx) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stold) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoldx) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stollx) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stolx) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoul) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoull) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoullx) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoulx) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Tolotab) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Touptab) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Towctrans) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unlockfilelock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Unlocksyslock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _WStod) {
    return UNIMPLEMENTED();
}

EXPORT(int, _WStof) {
    return UNIMPLEMENTED();
}

EXPORT(int, _WStold) {
    return UNIMPLEMENTED();
}

EXPORT(int, _WStoul) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Wctob) {
    return UNIMPLEMENTED();
}

EXPORT(int, _Wctomb) {
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_atexit) {
    return UNIMPLEMENTED();
}

EXPORT(int, __at_quick_exit) {
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_atexit) {
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_finalize) {
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_guard_abort) {
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_guard_acquire) {
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_guard_release) {
    return UNIMPLEMENTED();
}

EXPORT(void, __cxa_set_dso_handle_main, Ptr<void> dso) {
    //LOG_WARN("__cxa_set_dso_handle_main(dso=*0x%x)", dso);
    g_dso = dso;
    //return UNIMPLEMENTED();
}

EXPORT(int, __set_exidx_main) {
    return UNIMPLEMENTED();
}

EXPORT(int, __tls_get_addr) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceLdTlsRegisterModuleInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceLdTlsUnregisterModuleInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceLibcErrnoLoc) {
    return UNIMPLEMENTED();
}

EXPORT(int, abort) {
    return UNIMPLEMENTED();
}

EXPORT(int, abort_handler_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, abs) {
    return UNIMPLEMENTED();
}

EXPORT(int, asctime) {
    return UNIMPLEMENTED();
}

EXPORT(int, asctime_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, atof) {
    return UNIMPLEMENTED();
}

EXPORT(int, atoff) {
    return UNIMPLEMENTED();
}

EXPORT(int, atoi, const char *str) {
    return atoi(str);
}

EXPORT(int, atol) {
    return UNIMPLEMENTED();
}

EXPORT(int, atoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, bsearch) {
    return UNIMPLEMENTED();
}

EXPORT(int, bsearch_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, btowc) {
    return UNIMPLEMENTED();
}

EXPORT(int, c16rtomb) {
    return UNIMPLEMENTED();
}

EXPORT(int, c32rtomb) {
    return UNIMPLEMENTED();
}

EXPORT(int, calloc) {
    return UNIMPLEMENTED();
}

EXPORT(int, clearerr) {
    return UNIMPLEMENTED();
}

EXPORT(int, clock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ctime) {
    return UNIMPLEMENTED();
}

EXPORT(int, ctime_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, difftime) {
    return UNIMPLEMENTED();
}

EXPORT(int, div) {
    return UNIMPLEMENTED();
}

EXPORT(int, exit) {
    return UNIMPLEMENTED();
}

EXPORT(void, fclose, SceUID file) {
    close_file(host.io, file, export_name);
}

EXPORT(int, fdopen) {
    return UNIMPLEMENTED();
}

EXPORT(int, feof) {
    return UNIMPLEMENTED();
}

EXPORT(int, ferror) {
    return UNIMPLEMENTED();
}

EXPORT(int, fflush) {
    return UNIMPLEMENTED();
}

EXPORT(int, fgetc) {
    return UNIMPLEMENTED();
}

EXPORT(int, fgetpos) {
    return UNIMPLEMENTED();
}

EXPORT(int, fgets) {
    return UNIMPLEMENTED();
}

EXPORT(int, fgetwc) {
    return UNIMPLEMENTED();
}

EXPORT(int, fgetws) {
    return UNIMPLEMENTED();
}

EXPORT(int, fileno) {
    return UNIMPLEMENTED();
}

EXPORT(int, fopen, const char *filename, const char *mode) {
    LOG_WARN_IF(mode[0] != 'r', "fopen({}, {})", filename, *mode);
    return open_file(host.io, filename, SCE_O_RDONLY, host.pref_path, export_name);
}

EXPORT(int, fopen_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, fprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, fputc) {
    return UNIMPLEMENTED();
}

EXPORT(int, fputs) {
    return UNIMPLEMENTED();
}

EXPORT(int, fputwc) {
    return UNIMPLEMENTED();
}

EXPORT(int, fputws) {
    return UNIMPLEMENTED();
}

EXPORT(int, fread) {
    return UNIMPLEMENTED();
}

EXPORT(void, free, Address mem) {
    free(host.mem, mem);
}

EXPORT(int, freopen) {
    return UNIMPLEMENTED();
}

EXPORT(int, freopen_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, fscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, fseek) {
    return UNIMPLEMENTED();
}

EXPORT(int, fsetpos) {
    return UNIMPLEMENTED();
}

EXPORT(int, ftell) {
    return UNIMPLEMENTED();
}

EXPORT(int, fwide) {
    return UNIMPLEMENTED();
}

EXPORT(int, fwprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fwprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, fwrite) {
    return UNIMPLEMENTED();
}

EXPORT(int, fwscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, fwscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, getc) {
    return UNIMPLEMENTED();
}

EXPORT(int, getchar) {
    return UNIMPLEMENTED();
}

EXPORT(int, gets) {
    return UNIMPLEMENTED();
}

EXPORT(int, gets_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, getwc) {
    return UNIMPLEMENTED();
}

EXPORT(int, getwchar) {
    return UNIMPLEMENTED();
}

EXPORT(int, gmtime) {
    return UNIMPLEMENTED();
}

EXPORT(int, gmtime_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, ignore_handler_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, imaxabs) {
    return UNIMPLEMENTED();
}

EXPORT(int, imaxdiv) {
    return UNIMPLEMENTED();
}

EXPORT(int, isalnum) {
    return UNIMPLEMENTED();
}

EXPORT(int, isalpha) {
    return UNIMPLEMENTED();
}

EXPORT(int, isblank) {
    return UNIMPLEMENTED();
}

EXPORT(int, iscntrl) {
    return UNIMPLEMENTED();
}

EXPORT(int, isdigit) {
    return UNIMPLEMENTED();
}

EXPORT(int, isgraph) {
    return UNIMPLEMENTED();
}

EXPORT(int, islower) {
    return UNIMPLEMENTED();
}

EXPORT(int, isprint) {
    return UNIMPLEMENTED();
}

EXPORT(int, ispunct) {
    return UNIMPLEMENTED();
}

EXPORT(int, isspace) {
    return UNIMPLEMENTED();
}

EXPORT(int, isupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswalnum) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswalpha) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswblank) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswcntrl) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswctype) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswdigit) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswgraph) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswlower) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswprint) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswpunct) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswspace) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, iswxdigit) {
    return UNIMPLEMENTED();
}

EXPORT(int, isxdigit) {
    return UNIMPLEMENTED();
}

EXPORT(int, labs) {
    return UNIMPLEMENTED();
}

EXPORT(int, ldiv) {
    return UNIMPLEMENTED();
}

EXPORT(int, llabs) {
    return UNIMPLEMENTED();
}

EXPORT(int, lldiv) {
    return UNIMPLEMENTED();
}

EXPORT(int, localtime) {
    return UNIMPLEMENTED();
}

EXPORT(int, localtime_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, longjmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, malloc, SceSize size) {
    return alloc(host.mem, size, __FUNCTION__);
}

EXPORT(int, malloc_stats) {
    return UNIMPLEMENTED();
}

EXPORT(int, malloc_stats_fast) {
    return UNIMPLEMENTED();
}

EXPORT(int, malloc_usable_size) {
    return UNIMPLEMENTED();
}

EXPORT(int, mblen) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbrlen) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbrtoc16) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbrtoc32) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbrtowc) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbsinit) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbsrtowcs) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbsrtowcs_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbstowcs) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbstowcs_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, mbtowc) {
    return UNIMPLEMENTED();
}

EXPORT(int, memalign) {
    return UNIMPLEMENTED();
}

EXPORT(int, memchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, memcmp) {
    return UNIMPLEMENTED();
}

EXPORT(void, memcpy, void *destination, const void *source, size_t num) {
    memcpy(destination, source, num);
}

EXPORT(int, memcpy_s) {
    return UNIMPLEMENTED();
}

EXPORT(void, memmove, void *destination, const void *source, size_t num) {
    memmove(destination, source, num);
}

EXPORT(int, memmove_s) {
    return UNIMPLEMENTED();
}

EXPORT(void, memset, Ptr<void> str, int c, size_t n) {
    memset(str.get(host.mem), c, n);
}

EXPORT(int, mktime) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_calloc) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_create) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_create_with_flag) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_destroy) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_free) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_is_heap_empty) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_malloc) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_malloc_stats) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_malloc_stats_fast) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_malloc_usable_size) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_memalign) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_realloc) {
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_reallocalign) {
    return UNIMPLEMENTED();
}

EXPORT(int, perror) {
    return UNIMPLEMENTED();
}

EXPORT(int, printf, char *fmt, va_list va_args) {
    LOG_INFO("{}", fmt);
    return 0;
}

EXPORT(int, printf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, putc) {
    return UNIMPLEMENTED();
}

EXPORT(int, putchar) {
    return UNIMPLEMENTED();
}

EXPORT(int, puts) {
    return UNIMPLEMENTED();
}

EXPORT(int, putwc) {
    return UNIMPLEMENTED();
}

EXPORT(int, putwchar) {
    return UNIMPLEMENTED();
}

EXPORT(int, qsort) {
    return UNIMPLEMENTED();
}

EXPORT(int, qsort_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, quick_exit) {
    return UNIMPLEMENTED();
}

EXPORT(int, rand) {
    return UNIMPLEMENTED();
}

EXPORT(int, rand_r) {
    return UNIMPLEMENTED();
}

EXPORT(int, realloc) {
    return UNIMPLEMENTED();
}

EXPORT(int, reallocalign) {
    return UNIMPLEMENTED();
}

EXPORT(int, remove) {
    return UNIMPLEMENTED();
}

EXPORT(int, rename) {
    return UNIMPLEMENTED();
}

EXPORT(int, rewind) {
    return UNIMPLEMENTED();
}

EXPORT(int, scanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, scanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcFopenWithFD) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcFopenWithFH) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcGetFD) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcGetFH) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcSetHeapInitError) {
    return UNIMPLEMENTED();
}

EXPORT(int, set_constraint_handler_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, setbuf) {
    return UNIMPLEMENTED();
}

EXPORT(int, setjmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, setvbuf, FILE *stream, char *buffer, int mode, size_t size) {
    return UNIMPLEMENTED();
}

EXPORT(int, snprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, snprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, snwprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, sprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, srand) {
    return UNIMPLEMENTED();
}

EXPORT(int, sscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strcasecmp) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, strcat, Ptr<char> destination, Ptr<char> source) {
    strcat(destination.get(host.mem), source.get(host.mem));
    return destination;
}

EXPORT(int, strcat_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, strcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, strcoll) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, strcpy, Ptr<char> destination, Ptr<char> source) {
    strcpy(destination.get(host.mem), source.get(host.mem));
    return destination;
}

EXPORT(int, strcpy_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strcspn) {
    return UNIMPLEMENTED();
}

EXPORT(int, strdup) {
    return UNIMPLEMENTED();
}

EXPORT(int, strerror) {
    return UNIMPLEMENTED();
}

EXPORT(int, strerror_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strerrorlen_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strftime) {
    return UNIMPLEMENTED();
}

EXPORT(int, strlen, char *str) {
    return static_cast<int>(strlen(str));
}

EXPORT(int, strncasecmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, strncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, strncat_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strncmp, const char *str1, const char *str2, SceSize num) {
    return strncmp(str1, str2, num);
}

EXPORT(Ptr<char>, strncpy, Ptr<char> destination, Ptr<char> source, SceSize size) {
    strncpy(destination.get(host.mem), source.get(host.mem), size);
    return destination;
}

EXPORT(int, strncpy_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strnlen_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strpbrk) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, strrchr, Ptr<char> str, char ch) {
    Ptr<char> res = Ptr<char>();
    char *_str = str.get(host.mem);
    for (int i = static_cast<int>(strlen(_str) - 1); i >= 0; i--) {
        const char ch1 = _str[i];
        if (ch1 == ch) {
            res = str + i * sizeof(char);
            break;
        }
    }

    return res;
}

EXPORT(int, strspn) {
    return UNIMPLEMENTED();
}

EXPORT(int, strstr) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtod) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtof) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtoimax) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtok) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtok_r) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtok_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtol) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtold) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtoul) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtoull) {
    return UNIMPLEMENTED();
}

EXPORT(int, strtoumax) {
    return UNIMPLEMENTED();
}

EXPORT(int, strxfrm) {
    return UNIMPLEMENTED();
}

EXPORT(int, swprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, swprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, swscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, swscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, time) {
    return UNIMPLEMENTED();
}

EXPORT(int, tolower) {
    return UNIMPLEMENTED();
}

EXPORT(int, toupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, towctrans) {
    return UNIMPLEMENTED();
}

EXPORT(int, towlower) {
    return UNIMPLEMENTED();
}

EXPORT(int, towupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, ungetc) {
    return UNIMPLEMENTED();
}

EXPORT(int, ungetwc) {
    return UNIMPLEMENTED();
}

EXPORT(int, vfprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vfprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vfscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vfscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vfwprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vfwprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vfwscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vfwscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vsnprintf, char *s, size_t n, const char *format, va_list arg) {
    return snprintf(s, n, format, "");
}

EXPORT(int, vsnprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vsnwprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vsprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vsprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vsscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vsscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vswprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vswprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vswscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vswscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vwprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vwprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, vwscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, vwscanf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcrtomb) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcrtomb_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcscat) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcscat_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcschr) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcscmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcscoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcscpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcscpy_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcscspn) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsftime) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcslen) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncat_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncpy_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsnlen_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcspbrk) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsrtombs) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsrtombs_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsspn) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsstr) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstod) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstof) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoimax) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstok) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstok_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstol) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstold) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstombs) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstombs_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoul) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoull) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoumax) {
    return UNIMPLEMENTED();
}

EXPORT(int, wcsxfrm) {
    return UNIMPLEMENTED();
}

EXPORT(int, wctob) {
    return UNIMPLEMENTED();
}

EXPORT(int, wctomb) {
    return UNIMPLEMENTED();
}

EXPORT(int, wctomb_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wctrans) {
    return UNIMPLEMENTED();
}

EXPORT(int, wctype) {
    return UNIMPLEMENTED();
}

EXPORT(int, wmemchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, wmemcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, wmemcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, wmemcpy_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wmemmove) {
    return UNIMPLEMENTED();
}

EXPORT(int, wmemmove_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wmemset) {
    return UNIMPLEMENTED();
}

EXPORT(int, wprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, wprintf_s) {
    return UNIMPLEMENTED();
}

EXPORT(int, wscanf) {
    return UNIMPLEMENTED();
}

EXPORT(int, wscanf_s) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_Assert)
BRIDGE_IMPL(_Btowc)
BRIDGE_IMPL(_Ctype)
BRIDGE_IMPL(_Dbl)
BRIDGE_IMPL(_Denorm)
BRIDGE_IMPL(_Exit)
BRIDGE_IMPL(_FCbuild)
BRIDGE_IMPL(_FDenorm)
BRIDGE_IMPL(_FInf)
BRIDGE_IMPL(_FNan)
BRIDGE_IMPL(_FSnan)
BRIDGE_IMPL(_Files)
BRIDGE_IMPL(_Flt)
BRIDGE_IMPL(_Fltrounds)
BRIDGE_IMPL(_Inf)
BRIDGE_IMPL(_Iswctype)
BRIDGE_IMPL(_LDenorm)
BRIDGE_IMPL(_LInf)
BRIDGE_IMPL(_LNan)
BRIDGE_IMPL(_LSnan)
BRIDGE_IMPL(_Ldbl)
BRIDGE_IMPL(_Lockfilelock)
BRIDGE_IMPL(_Locksyslock)
BRIDGE_IMPL(_Mbtowc)
BRIDGE_IMPL(_Nan)
BRIDGE_IMPL(_PJP_C_Copyright)
BRIDGE_IMPL(_SCE_Assert)
BRIDGE_IMPL(_Snan)
BRIDGE_IMPL(_Stderr)
BRIDGE_IMPL(_Stdin)
BRIDGE_IMPL(_Stdout)
BRIDGE_IMPL(_Stod)
BRIDGE_IMPL(_Stodx)
BRIDGE_IMPL(_Stof)
BRIDGE_IMPL(_Stofx)
BRIDGE_IMPL(_Stold)
BRIDGE_IMPL(_Stoldx)
BRIDGE_IMPL(_Stoll)
BRIDGE_IMPL(_Stollx)
BRIDGE_IMPL(_Stolx)
BRIDGE_IMPL(_Stoul)
BRIDGE_IMPL(_Stoull)
BRIDGE_IMPL(_Stoullx)
BRIDGE_IMPL(_Stoulx)
BRIDGE_IMPL(_Tolotab)
BRIDGE_IMPL(_Touptab)
BRIDGE_IMPL(_Towctrans)
BRIDGE_IMPL(_Unlockfilelock)
BRIDGE_IMPL(_Unlocksyslock)
BRIDGE_IMPL(_WStod)
BRIDGE_IMPL(_WStof)
BRIDGE_IMPL(_WStold)
BRIDGE_IMPL(_WStoul)
BRIDGE_IMPL(_Wctob)
BRIDGE_IMPL(_Wctomb)
BRIDGE_IMPL(__aeabi_atexit)
BRIDGE_IMPL(__at_quick_exit)
BRIDGE_IMPL(__cxa_atexit)
BRIDGE_IMPL(__cxa_finalize)
BRIDGE_IMPL(__cxa_guard_abort)
BRIDGE_IMPL(__cxa_guard_acquire)
BRIDGE_IMPL(__cxa_guard_release)
BRIDGE_IMPL(__cxa_set_dso_handle_main)
BRIDGE_IMPL(__set_exidx_main)
BRIDGE_IMPL(__tls_get_addr)
BRIDGE_IMPL(_sceLdTlsRegisterModuleInfo)
BRIDGE_IMPL(_sceLdTlsUnregisterModuleInfo)
BRIDGE_IMPL(_sceLibcErrnoLoc)
BRIDGE_IMPL(abort)
BRIDGE_IMPL(abort_handler_s)
BRIDGE_IMPL(abs)
BRIDGE_IMPL(asctime)
BRIDGE_IMPL(asctime_s)
BRIDGE_IMPL(atof)
BRIDGE_IMPL(atoff)
BRIDGE_IMPL(atoi)
BRIDGE_IMPL(atol)
BRIDGE_IMPL(atoll)
BRIDGE_IMPL(bsearch)
BRIDGE_IMPL(bsearch_s)
BRIDGE_IMPL(btowc)
BRIDGE_IMPL(c16rtomb)
BRIDGE_IMPL(c32rtomb)
BRIDGE_IMPL(calloc)
BRIDGE_IMPL(clearerr)
BRIDGE_IMPL(clock)
BRIDGE_IMPL(ctime)
BRIDGE_IMPL(ctime_s)
BRIDGE_IMPL(difftime)
BRIDGE_IMPL(div)
BRIDGE_IMPL(exit)
BRIDGE_IMPL(fclose)
BRIDGE_IMPL(fdopen)
BRIDGE_IMPL(feof)
BRIDGE_IMPL(ferror)
BRIDGE_IMPL(fflush)
BRIDGE_IMPL(fgetc)
BRIDGE_IMPL(fgetpos)
BRIDGE_IMPL(fgets)
BRIDGE_IMPL(fgetwc)
BRIDGE_IMPL(fgetws)
BRIDGE_IMPL(fileno)
BRIDGE_IMPL(fopen)
BRIDGE_IMPL(fopen_s)
BRIDGE_IMPL(fprintf)
BRIDGE_IMPL(fprintf_s)
BRIDGE_IMPL(fputc)
BRIDGE_IMPL(fputs)
BRIDGE_IMPL(fputwc)
BRIDGE_IMPL(fputws)
BRIDGE_IMPL(fread)
BRIDGE_IMPL(free)
BRIDGE_IMPL(freopen)
BRIDGE_IMPL(freopen_s)
BRIDGE_IMPL(fscanf)
BRIDGE_IMPL(fscanf_s)
BRIDGE_IMPL(fseek)
BRIDGE_IMPL(fsetpos)
BRIDGE_IMPL(ftell)
BRIDGE_IMPL(fwide)
BRIDGE_IMPL(fwprintf)
BRIDGE_IMPL(fwprintf_s)
BRIDGE_IMPL(fwrite)
BRIDGE_IMPL(fwscanf)
BRIDGE_IMPL(fwscanf_s)
BRIDGE_IMPL(getc)
BRIDGE_IMPL(getchar)
BRIDGE_IMPL(gets)
BRIDGE_IMPL(gets_s)
BRIDGE_IMPL(getwc)
BRIDGE_IMPL(getwchar)
BRIDGE_IMPL(gmtime)
BRIDGE_IMPL(gmtime_s)
BRIDGE_IMPL(ignore_handler_s)
BRIDGE_IMPL(imaxabs)
BRIDGE_IMPL(imaxdiv)
BRIDGE_IMPL(isalnum)
BRIDGE_IMPL(isalpha)
BRIDGE_IMPL(isblank)
BRIDGE_IMPL(iscntrl)
BRIDGE_IMPL(isdigit)
BRIDGE_IMPL(isgraph)
BRIDGE_IMPL(islower)
BRIDGE_IMPL(isprint)
BRIDGE_IMPL(ispunct)
BRIDGE_IMPL(isspace)
BRIDGE_IMPL(isupper)
BRIDGE_IMPL(iswalnum)
BRIDGE_IMPL(iswalpha)
BRIDGE_IMPL(iswblank)
BRIDGE_IMPL(iswcntrl)
BRIDGE_IMPL(iswctype)
BRIDGE_IMPL(iswdigit)
BRIDGE_IMPL(iswgraph)
BRIDGE_IMPL(iswlower)
BRIDGE_IMPL(iswprint)
BRIDGE_IMPL(iswpunct)
BRIDGE_IMPL(iswspace)
BRIDGE_IMPL(iswupper)
BRIDGE_IMPL(iswxdigit)
BRIDGE_IMPL(isxdigit)
BRIDGE_IMPL(labs)
BRIDGE_IMPL(ldiv)
BRIDGE_IMPL(llabs)
BRIDGE_IMPL(lldiv)
BRIDGE_IMPL(localtime)
BRIDGE_IMPL(localtime_s)
BRIDGE_IMPL(longjmp)
BRIDGE_IMPL(malloc)
BRIDGE_IMPL(malloc_stats)
BRIDGE_IMPL(malloc_stats_fast)
BRIDGE_IMPL(malloc_usable_size)
BRIDGE_IMPL(mblen)
BRIDGE_IMPL(mbrlen)
BRIDGE_IMPL(mbrtoc16)
BRIDGE_IMPL(mbrtoc32)
BRIDGE_IMPL(mbrtowc)
BRIDGE_IMPL(mbsinit)
BRIDGE_IMPL(mbsrtowcs)
BRIDGE_IMPL(mbsrtowcs_s)
BRIDGE_IMPL(mbstowcs)
BRIDGE_IMPL(mbstowcs_s)
BRIDGE_IMPL(mbtowc)
BRIDGE_IMPL(memalign)
BRIDGE_IMPL(memchr)
BRIDGE_IMPL(memcmp)
BRIDGE_IMPL(memcpy)
BRIDGE_IMPL(memcpy_s)
BRIDGE_IMPL(memmove)
BRIDGE_IMPL(memmove_s)
BRIDGE_IMPL(memset)
BRIDGE_IMPL(mktime)
BRIDGE_IMPL(mspace_calloc)
BRIDGE_IMPL(mspace_create)
BRIDGE_IMPL(mspace_create_with_flag)
BRIDGE_IMPL(mspace_destroy)
BRIDGE_IMPL(mspace_free)
BRIDGE_IMPL(mspace_is_heap_empty)
BRIDGE_IMPL(mspace_malloc)
BRIDGE_IMPL(mspace_malloc_stats)
BRIDGE_IMPL(mspace_malloc_stats_fast)
BRIDGE_IMPL(mspace_malloc_usable_size)
BRIDGE_IMPL(mspace_memalign)
BRIDGE_IMPL(mspace_realloc)
BRIDGE_IMPL(mspace_reallocalign)
BRIDGE_IMPL(perror)
BRIDGE_IMPL(printf)
BRIDGE_IMPL(printf_s)
BRIDGE_IMPL(putc)
BRIDGE_IMPL(putchar)
BRIDGE_IMPL(puts)
BRIDGE_IMPL(putwc)
BRIDGE_IMPL(putwchar)
BRIDGE_IMPL(qsort)
BRIDGE_IMPL(qsort_s)
BRIDGE_IMPL(quick_exit)
BRIDGE_IMPL(rand)
BRIDGE_IMPL(rand_r)
BRIDGE_IMPL(realloc)
BRIDGE_IMPL(reallocalign)
BRIDGE_IMPL(remove)
BRIDGE_IMPL(rename)
BRIDGE_IMPL(rewind)
BRIDGE_IMPL(scanf)
BRIDGE_IMPL(scanf_s)
BRIDGE_IMPL(sceLibcFopenWithFD)
BRIDGE_IMPL(sceLibcFopenWithFH)
BRIDGE_IMPL(sceLibcGetFD)
BRIDGE_IMPL(sceLibcGetFH)
BRIDGE_IMPL(sceLibcSetHeapInitError)
BRIDGE_IMPL(set_constraint_handler_s)
BRIDGE_IMPL(setbuf)
BRIDGE_IMPL(setjmp)
BRIDGE_IMPL(setvbuf)
BRIDGE_IMPL(snprintf)
BRIDGE_IMPL(snprintf_s)
BRIDGE_IMPL(snwprintf_s)
BRIDGE_IMPL(sprintf)
BRIDGE_IMPL(sprintf_s)
BRIDGE_IMPL(srand)
BRIDGE_IMPL(sscanf)
BRIDGE_IMPL(sscanf_s)
BRIDGE_IMPL(strcasecmp)
BRIDGE_IMPL(strcat)
BRIDGE_IMPL(strcat_s)
BRIDGE_IMPL(strchr)
BRIDGE_IMPL(strcmp)
BRIDGE_IMPL(strcoll)
BRIDGE_IMPL(strcpy)
BRIDGE_IMPL(strcpy_s)
BRIDGE_IMPL(strcspn)
BRIDGE_IMPL(strdup)
BRIDGE_IMPL(strerror)
BRIDGE_IMPL(strerror_s)
BRIDGE_IMPL(strerrorlen_s)
BRIDGE_IMPL(strftime)
BRIDGE_IMPL(strlen)
BRIDGE_IMPL(strncasecmp)
BRIDGE_IMPL(strncat)
BRIDGE_IMPL(strncat_s)
BRIDGE_IMPL(strncmp)
BRIDGE_IMPL(strncpy)
BRIDGE_IMPL(strncpy_s)
BRIDGE_IMPL(strnlen_s)
BRIDGE_IMPL(strpbrk)
BRIDGE_IMPL(strrchr)
BRIDGE_IMPL(strspn)
BRIDGE_IMPL(strstr)
BRIDGE_IMPL(strtod)
BRIDGE_IMPL(strtof)
BRIDGE_IMPL(strtoimax)
BRIDGE_IMPL(strtok)
BRIDGE_IMPL(strtok_r)
BRIDGE_IMPL(strtok_s)
BRIDGE_IMPL(strtol)
BRIDGE_IMPL(strtold)
BRIDGE_IMPL(strtoll)
BRIDGE_IMPL(strtoul)
BRIDGE_IMPL(strtoull)
BRIDGE_IMPL(strtoumax)
BRIDGE_IMPL(strxfrm)
BRIDGE_IMPL(swprintf)
BRIDGE_IMPL(swprintf_s)
BRIDGE_IMPL(swscanf)
BRIDGE_IMPL(swscanf_s)
BRIDGE_IMPL(time)
BRIDGE_IMPL(tolower)
BRIDGE_IMPL(toupper)
BRIDGE_IMPL(towctrans)
BRIDGE_IMPL(towlower)
BRIDGE_IMPL(towupper)
BRIDGE_IMPL(ungetc)
BRIDGE_IMPL(ungetwc)
BRIDGE_IMPL(vfprintf)
BRIDGE_IMPL(vfprintf_s)
BRIDGE_IMPL(vfscanf)
BRIDGE_IMPL(vfscanf_s)
BRIDGE_IMPL(vfwprintf)
BRIDGE_IMPL(vfwprintf_s)
BRIDGE_IMPL(vfwscanf)
BRIDGE_IMPL(vfwscanf_s)
BRIDGE_IMPL(vprintf)
BRIDGE_IMPL(vprintf_s)
BRIDGE_IMPL(vscanf)
BRIDGE_IMPL(vscanf_s)
BRIDGE_IMPL(vsnprintf)
BRIDGE_IMPL(vsnprintf_s)
BRIDGE_IMPL(vsnwprintf_s)
BRIDGE_IMPL(vsprintf)
BRIDGE_IMPL(vsprintf_s)
BRIDGE_IMPL(vsscanf)
BRIDGE_IMPL(vsscanf_s)
BRIDGE_IMPL(vswprintf)
BRIDGE_IMPL(vswprintf_s)
BRIDGE_IMPL(vswscanf)
BRIDGE_IMPL(vswscanf_s)
BRIDGE_IMPL(vwprintf)
BRIDGE_IMPL(vwprintf_s)
BRIDGE_IMPL(vwscanf)
BRIDGE_IMPL(vwscanf_s)
BRIDGE_IMPL(wcrtomb)
BRIDGE_IMPL(wcrtomb_s)
BRIDGE_IMPL(wcscat)
BRIDGE_IMPL(wcscat_s)
BRIDGE_IMPL(wcschr)
BRIDGE_IMPL(wcscmp)
BRIDGE_IMPL(wcscoll)
BRIDGE_IMPL(wcscpy)
BRIDGE_IMPL(wcscpy_s)
BRIDGE_IMPL(wcscspn)
BRIDGE_IMPL(wcsftime)
BRIDGE_IMPL(wcslen)
BRIDGE_IMPL(wcsncat)
BRIDGE_IMPL(wcsncat_s)
BRIDGE_IMPL(wcsncmp)
BRIDGE_IMPL(wcsncpy)
BRIDGE_IMPL(wcsncpy_s)
BRIDGE_IMPL(wcsnlen_s)
BRIDGE_IMPL(wcspbrk)
BRIDGE_IMPL(wcsrchr)
BRIDGE_IMPL(wcsrtombs)
BRIDGE_IMPL(wcsrtombs_s)
BRIDGE_IMPL(wcsspn)
BRIDGE_IMPL(wcsstr)
BRIDGE_IMPL(wcstod)
BRIDGE_IMPL(wcstof)
BRIDGE_IMPL(wcstoimax)
BRIDGE_IMPL(wcstok)
BRIDGE_IMPL(wcstok_s)
BRIDGE_IMPL(wcstol)
BRIDGE_IMPL(wcstold)
BRIDGE_IMPL(wcstoll)
BRIDGE_IMPL(wcstombs)
BRIDGE_IMPL(wcstombs_s)
BRIDGE_IMPL(wcstoul)
BRIDGE_IMPL(wcstoull)
BRIDGE_IMPL(wcstoumax)
BRIDGE_IMPL(wcsxfrm)
BRIDGE_IMPL(wctob)
BRIDGE_IMPL(wctomb)
BRIDGE_IMPL(wctomb_s)
BRIDGE_IMPL(wctrans)
BRIDGE_IMPL(wctype)
BRIDGE_IMPL(wmemchr)
BRIDGE_IMPL(wmemcmp)
BRIDGE_IMPL(wmemcpy)
BRIDGE_IMPL(wmemcpy_s)
BRIDGE_IMPL(wmemmove)
BRIDGE_IMPL(wmemmove_s)
BRIDGE_IMPL(wmemset)
BRIDGE_IMPL(wprintf)
BRIDGE_IMPL(wprintf_s)
BRIDGE_IMPL(wscanf)
BRIDGE_IMPL(wscanf_s)
