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
    return unimplemented(export_name);
}

EXPORT(int, _Btowc) {
    return unimplemented(export_name);
}

EXPORT(int, _Ctype) {
    return unimplemented(export_name);
}

EXPORT(int, _Dbl) {
    return unimplemented(export_name);
}

EXPORT(int, _Denorm) {
    return unimplemented(export_name);
}

EXPORT(int, _Exit) {
    return unimplemented(export_name);
}

EXPORT(int, _FCbuild) {
    return unimplemented(export_name);
}

EXPORT(int, _FDenorm) {
    return unimplemented(export_name);
}

EXPORT(int, _FInf) {
    return unimplemented(export_name);
}

EXPORT(int, _FNan) {
    return unimplemented(export_name);
}

EXPORT(int, _FSnan) {
    return unimplemented(export_name);
}

EXPORT(int, _Files) {
    return unimplemented(export_name);
}

EXPORT(int, _Flt) {
    return unimplemented(export_name);
}

EXPORT(int, _Fltrounds) {
    return unimplemented(export_name);
}

EXPORT(int, _Inf) {
    return unimplemented(export_name);
}

EXPORT(int, _Iswctype) {
    return unimplemented(export_name);
}

EXPORT(int, _LDenorm) {
    return unimplemented(export_name);
}

EXPORT(int, _LInf) {
    return unimplemented(export_name);
}

EXPORT(int, _LNan) {
    return unimplemented(export_name);
}

EXPORT(int, _LSnan) {
    return unimplemented(export_name);
}

EXPORT(int, _Ldbl) {
    return unimplemented(export_name);
}

EXPORT(int, _Lockfilelock) {
    return unimplemented(export_name);
}

EXPORT(int, _Locksyslock) {
    return unimplemented(export_name);
}

EXPORT(int, _Mbtowc) {
    return unimplemented(export_name);
}

EXPORT(int, _Nan) {
    return unimplemented(export_name);
}

EXPORT(int, _PJP_C_Copyright) {
    return unimplemented(export_name);
}

EXPORT(int, _SCE_Assert) {
    return unimplemented(export_name);
}

EXPORT(int, _Snan) {
    return unimplemented(export_name);
}

EXPORT(int, _Stderr) {
    return unimplemented(export_name);
}

EXPORT(int, _Stdin) {
    return unimplemented(export_name);
}

EXPORT(int, _Stdout) {
    return unimplemented(export_name);
}

EXPORT(int, _Stod) {
    return unimplemented(export_name);
}

EXPORT(int, _Stodx) {
    return unimplemented(export_name);
}

EXPORT(int, _Stof) {
    return unimplemented(export_name);
}

EXPORT(int, _Stofx) {
    return unimplemented(export_name);
}

EXPORT(int, _Stold) {
    return unimplemented(export_name);
}

EXPORT(int, _Stoldx) {
    return unimplemented(export_name);
}

EXPORT(int, _Stoll) {
    return unimplemented(export_name);
}

EXPORT(int, _Stollx) {
    return unimplemented(export_name);
}

EXPORT(int, _Stolx) {
    return unimplemented(export_name);
}

EXPORT(int, _Stoul) {
    return unimplemented(export_name);
}

EXPORT(int, _Stoull) {
    return unimplemented(export_name);
}

EXPORT(int, _Stoullx) {
    return unimplemented(export_name);
}

EXPORT(int, _Stoulx) {
    return unimplemented(export_name);
}

EXPORT(int, _Tolotab) {
    return unimplemented(export_name);
}

EXPORT(int, _Touptab) {
    return unimplemented(export_name);
}

EXPORT(int, _Towctrans) {
    return unimplemented(export_name);
}

EXPORT(int, _Unlockfilelock) {
    return unimplemented(export_name);
}

EXPORT(int, _Unlocksyslock) {
    return unimplemented(export_name);
}

EXPORT(int, _WStod) {
    return unimplemented(export_name);
}

EXPORT(int, _WStof) {
    return unimplemented(export_name);
}

EXPORT(int, _WStold) {
    return unimplemented(export_name);
}

EXPORT(int, _WStoul) {
    return unimplemented(export_name);
}

EXPORT(int, _Wctob) {
    return unimplemented(export_name);
}

EXPORT(int, _Wctomb) {
    return unimplemented(export_name);
}

EXPORT(int, __aeabi_atexit) {
    return unimplemented(export_name);
}

EXPORT(int, __at_quick_exit) {
    return unimplemented(export_name);
}

EXPORT(int, __cxa_atexit) {
    return unimplemented(export_name);
}

EXPORT(int, __cxa_finalize) {
    return unimplemented(export_name);
}

EXPORT(int, __cxa_guard_abort) {
    return unimplemented(export_name);
}

EXPORT(int, __cxa_guard_acquire) {
    return unimplemented(export_name);
}

EXPORT(int, __cxa_guard_release) {
    return unimplemented(export_name);
}

EXPORT(void, __cxa_set_dso_handle_main, Ptr<void> dso) {
    //LOG_WARN("__cxa_set_dso_handle_main(dso=*0x%x)", dso);
    g_dso = dso;
    //return unimplemented(export_name);
}

EXPORT(int, __set_exidx_main) {
    return unimplemented(export_name);
}

EXPORT(int, __tls_get_addr) {
    return unimplemented(export_name);
}

EXPORT(int, _sceLdTlsRegisterModuleInfo) {
    return unimplemented(export_name);
}

EXPORT(int, _sceLdTlsUnregisterModuleInfo) {
    return unimplemented(export_name);
}

EXPORT(int, _sceLibcErrnoLoc) {
    return unimplemented(export_name);
}

EXPORT(int, abort) {
    return unimplemented(export_name);
}

EXPORT(int, abort_handler_s) {
    return unimplemented(export_name);
}

EXPORT(int, abs) {
    return unimplemented(export_name);
}

EXPORT(int, asctime) {
    return unimplemented(export_name);
}

EXPORT(int, asctime_s) {
    return unimplemented(export_name);
}

EXPORT(int, atof) {
    return unimplemented(export_name);
}

EXPORT(int, atoff) {
    return unimplemented(export_name);
}

EXPORT(int, atoi, const char *str) {
    return atoi(str);
}

EXPORT(int, atol) {
    return unimplemented(export_name);
}

EXPORT(int, atoll) {
    return unimplemented(export_name);
}

EXPORT(int, bsearch) {
    return unimplemented(export_name);
}

EXPORT(int, bsearch_s) {
    return unimplemented(export_name);
}

EXPORT(int, btowc) {
    return unimplemented(export_name);
}

EXPORT(int, c16rtomb) {
    return unimplemented(export_name);
}

EXPORT(int, c32rtomb) {
    return unimplemented(export_name);
}

EXPORT(int, calloc) {
    return unimplemented(export_name);
}

EXPORT(int, clearerr) {
    return unimplemented(export_name);
}

EXPORT(int, clock) {
    return unimplemented(export_name);
}

EXPORT(int, ctime) {
    return unimplemented(export_name);
}

EXPORT(int, ctime_s) {
    return unimplemented(export_name);
}

EXPORT(int, difftime) {
    return unimplemented(export_name);
}

EXPORT(int, div) {
    return unimplemented(export_name);
}

EXPORT(int, exit) {
    return unimplemented(export_name);
}

EXPORT(void, fclose, SceUID file) {
    close_file(host.io, file);
}

EXPORT(int, fdopen) {
    return unimplemented(export_name);
}

EXPORT(int, feof) {
    return unimplemented(export_name);
}

EXPORT(int, ferror) {
    return unimplemented(export_name);
}

EXPORT(int, fflush) {
    return unimplemented(export_name);
}

EXPORT(int, fgetc) {
    return unimplemented(export_name);
}

EXPORT(int, fgetpos) {
    return unimplemented(export_name);
}

EXPORT(int, fgets) {
    return unimplemented(export_name);
}

EXPORT(int, fgetwc) {
    return unimplemented(export_name);
}

EXPORT(int, fgetws) {
    return unimplemented(export_name);
}

EXPORT(int, fileno) {
    return unimplemented(export_name);
}

EXPORT(int, fopen, const char *filename, const char *mode) {
    return open_file(host.io, filename, SCE_O_RDONLY, host.pref_path.c_str());
}

EXPORT(int, fopen_s) {
    return unimplemented(export_name);
}

EXPORT(int, fprintf) {
    return unimplemented(export_name);
}

EXPORT(int, fprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, fputc) {
    return unimplemented(export_name);
}

EXPORT(int, fputs) {
    return unimplemented(export_name);
}

EXPORT(int, fputwc) {
    return unimplemented(export_name);
}

EXPORT(int, fputws) {
    return unimplemented(export_name);
}

EXPORT(int, fread) {
    return unimplemented(export_name);
}

EXPORT(void, free, Address mem) {
    free(host.mem, mem);
}

EXPORT(int, freopen) {
    return unimplemented(export_name);
}

EXPORT(int, freopen_s) {
    return unimplemented(export_name);
}

EXPORT(int, fscanf) {
    return unimplemented(export_name);
}

EXPORT(int, fscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, fseek) {
    return unimplemented(export_name);
}

EXPORT(int, fsetpos) {
    return unimplemented(export_name);
}

EXPORT(int, ftell) {
    return unimplemented(export_name);
}

EXPORT(int, fwide) {
    return unimplemented(export_name);
}

EXPORT(int, fwprintf) {
    return unimplemented(export_name);
}

EXPORT(int, fwprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, fwrite) {
    return unimplemented(export_name);
}

EXPORT(int, fwscanf) {
    return unimplemented(export_name);
}

EXPORT(int, fwscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, getc) {
    return unimplemented(export_name);
}

EXPORT(int, getchar) {
    return unimplemented(export_name);
}

EXPORT(int, gets) {
    return unimplemented(export_name);
}

EXPORT(int, gets_s) {
    return unimplemented(export_name);
}

EXPORT(int, getwc) {
    return unimplemented(export_name);
}

EXPORT(int, getwchar) {
    return unimplemented(export_name);
}

EXPORT(int, gmtime) {
    return unimplemented(export_name);
}

EXPORT(int, gmtime_s) {
    return unimplemented(export_name);
}

EXPORT(int, ignore_handler_s) {
    return unimplemented(export_name);
}

EXPORT(int, imaxabs) {
    return unimplemented(export_name);
}

EXPORT(int, imaxdiv) {
    return unimplemented(export_name);
}

EXPORT(int, isalnum) {
    return unimplemented(export_name);
}

EXPORT(int, isalpha) {
    return unimplemented(export_name);
}

EXPORT(int, isblank) {
    return unimplemented(export_name);
}

EXPORT(int, iscntrl) {
    return unimplemented(export_name);
}

EXPORT(int, isdigit) {
    return unimplemented(export_name);
}

EXPORT(int, isgraph) {
    return unimplemented(export_name);
}

EXPORT(int, islower) {
    return unimplemented(export_name);
}

EXPORT(int, isprint) {
    return unimplemented(export_name);
}

EXPORT(int, ispunct) {
    return unimplemented(export_name);
}

EXPORT(int, isspace) {
    return unimplemented(export_name);
}

EXPORT(int, isupper) {
    return unimplemented(export_name);
}

EXPORT(int, iswalnum) {
    return unimplemented(export_name);
}

EXPORT(int, iswalpha) {
    return unimplemented(export_name);
}

EXPORT(int, iswblank) {
    return unimplemented(export_name);
}

EXPORT(int, iswcntrl) {
    return unimplemented(export_name);
}

EXPORT(int, iswctype) {
    return unimplemented(export_name);
}

EXPORT(int, iswdigit) {
    return unimplemented(export_name);
}

EXPORT(int, iswgraph) {
    return unimplemented(export_name);
}

EXPORT(int, iswlower) {
    return unimplemented(export_name);
}

EXPORT(int, iswprint) {
    return unimplemented(export_name);
}

EXPORT(int, iswpunct) {
    return unimplemented(export_name);
}

EXPORT(int, iswspace) {
    return unimplemented(export_name);
}

EXPORT(int, iswupper) {
    return unimplemented(export_name);
}

EXPORT(int, iswxdigit) {
    return unimplemented(export_name);
}

EXPORT(int, isxdigit) {
    return unimplemented(export_name);
}

EXPORT(int, labs) {
    return unimplemented(export_name);
}

EXPORT(int, ldiv) {
    return unimplemented(export_name);
}

EXPORT(int, llabs) {
    return unimplemented(export_name);
}

EXPORT(int, lldiv) {
    return unimplemented(export_name);
}

EXPORT(int, localtime) {
    return unimplemented(export_name);
}

EXPORT(int, localtime_s) {
    return unimplemented(export_name);
}

EXPORT(int, longjmp) {
    return unimplemented(export_name);
}

EXPORT(int, malloc, SceSize size) {
    return alloc(host.mem, size, "");
}

EXPORT(int, malloc_stats) {
    return unimplemented(export_name);
}

EXPORT(int, malloc_stats_fast) {
    return unimplemented(export_name);
}

EXPORT(int, malloc_usable_size) {
    return unimplemented(export_name);
}

EXPORT(int, mblen) {
    return unimplemented(export_name);
}

EXPORT(int, mbrlen) {
    return unimplemented(export_name);
}

EXPORT(int, mbrtoc16) {
    return unimplemented(export_name);
}

EXPORT(int, mbrtoc32) {
    return unimplemented(export_name);
}

EXPORT(int, mbrtowc) {
    return unimplemented(export_name);
}

EXPORT(int, mbsinit) {
    return unimplemented(export_name);
}

EXPORT(int, mbsrtowcs) {
    return unimplemented(export_name);
}

EXPORT(int, mbsrtowcs_s) {
    return unimplemented(export_name);
}

EXPORT(int, mbstowcs) {
    return unimplemented(export_name);
}

EXPORT(int, mbstowcs_s) {
    return unimplemented(export_name);
}

EXPORT(int, mbtowc) {
    return unimplemented(export_name);
}

EXPORT(int, memalign) {
    return unimplemented(export_name);
}

EXPORT(int, memchr) {
    return unimplemented(export_name);
}

EXPORT(int, memcmp) {
    return unimplemented(export_name);
}

EXPORT(void, memcpy, void *destination, const void *source, size_t num) {
    memcpy(destination, source, num);
}

EXPORT(int, memcpy_s) {
    return unimplemented(export_name);
}

EXPORT(void, memmove, void *destination, const void *source, size_t num) {
    memmove(destination, source, num);
}

EXPORT(int, memmove_s) {
    return unimplemented(export_name);
}

EXPORT(void, memset, Ptr<void> str, int c, size_t n) {
    memset(str.get(host.mem), c, n);
}

EXPORT(int, mktime) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_calloc) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_create) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_create_with_flag) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_destroy) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_free) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_is_heap_empty) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_malloc) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_malloc_stats) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_malloc_stats_fast) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_malloc_usable_size) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_memalign) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_realloc) {
    return unimplemented(export_name);
}

EXPORT(int, mspace_reallocalign) {
    return unimplemented(export_name);
}

EXPORT(int, perror) {
    return unimplemented(export_name);
}

EXPORT(int, printf, char *fmt, va_list va_args) {
    LOG_INFO("{}", fmt);
    return 0;
}

EXPORT(int, printf_s) {
    return unimplemented(export_name);
}

EXPORT(int, putc) {
    return unimplemented(export_name);
}

EXPORT(int, putchar) {
    return unimplemented(export_name);
}

EXPORT(int, puts) {
    return unimplemented(export_name);
}

EXPORT(int, putwc) {
    return unimplemented(export_name);
}

EXPORT(int, putwchar) {
    return unimplemented(export_name);
}

EXPORT(int, qsort) {
    return unimplemented(export_name);
}

EXPORT(int, qsort_s) {
    return unimplemented(export_name);
}

EXPORT(int, quick_exit) {
    return unimplemented(export_name);
}

EXPORT(int, rand) {
    return unimplemented(export_name);
}

EXPORT(int, rand_r) {
    return unimplemented(export_name);
}

EXPORT(int, realloc) {
    return unimplemented(export_name);
}

EXPORT(int, reallocalign) {
    return unimplemented(export_name);
}

EXPORT(int, remove) {
    return unimplemented(export_name);
}

EXPORT(int, rename) {
    return unimplemented(export_name);
}

EXPORT(int, rewind) {
    return unimplemented(export_name);
}

EXPORT(int, scanf) {
    return unimplemented(export_name);
}

EXPORT(int, scanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, sceLibcFopenWithFD) {
    return unimplemented(export_name);
}

EXPORT(int, sceLibcFopenWithFH) {
    return unimplemented(export_name);
}

EXPORT(int, sceLibcGetFD) {
    return unimplemented(export_name);
}

EXPORT(int, sceLibcGetFH) {
    return unimplemented(export_name);
}

EXPORT(int, sceLibcSetHeapInitError) {
    return unimplemented(export_name);
}

EXPORT(int, set_constraint_handler_s) {
    return unimplemented(export_name);
}

EXPORT(int, setbuf) {
    return unimplemented(export_name);
}

EXPORT(int, setjmp) {
    return unimplemented(export_name);
}

EXPORT(int, setvbuf, FILE *stream, char *buffer, int mode, size_t size) {
    return unimplemented(export_name);
}

EXPORT(int, snprintf) {
    return unimplemented(export_name);
}

EXPORT(int, snprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, snwprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, sprintf) {
    return unimplemented(export_name);
}

EXPORT(int, sprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, srand) {
    return unimplemented(export_name);
}

EXPORT(int, sscanf) {
    return unimplemented(export_name);
}

EXPORT(int, sscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, strcasecmp) {
    return unimplemented(export_name);
}

EXPORT(Ptr<char>, strcat, Ptr<char> destination, Ptr<char> source) {
    strcat(destination.get(host.mem), source.get(host.mem));
    return destination;
}

EXPORT(int, strcat_s) {
    return unimplemented(export_name);
}

EXPORT(int, strchr) {
    return unimplemented(export_name);
}

EXPORT(int, strcmp) {
    return unimplemented(export_name);
}

EXPORT(int, strcoll) {
    return unimplemented(export_name);
}

EXPORT(Ptr<char>, strcpy, Ptr<char> destination, Ptr<char> source) {
    strcpy(destination.get(host.mem), source.get(host.mem));
    return destination;
}

EXPORT(int, strcpy_s) {
    return unimplemented(export_name);
}

EXPORT(int, strcspn) {
    return unimplemented(export_name);
}

EXPORT(int, strdup) {
    return unimplemented(export_name);
}

EXPORT(int, strerror) {
    return unimplemented(export_name);
}

EXPORT(int, strerror_s) {
    return unimplemented(export_name);
}

EXPORT(int, strerrorlen_s) {
    return unimplemented(export_name);
}

EXPORT(int, strftime) {
    return unimplemented(export_name);
}

EXPORT(int, strlen, char *str) {
    return strlen(str);
}

EXPORT(int, strncasecmp) {
    return unimplemented(export_name);
}

EXPORT(int, strncat) {
    return unimplemented(export_name);
}

EXPORT(int, strncat_s) {
    return unimplemented(export_name);
}

EXPORT(int, strncmp, const char *str1, const char *str2, SceSize num) {
    return strncmp(str1, str2, num);
}

EXPORT(Ptr<char>, strncpy, Ptr<char> destination, Ptr<char> source, SceSize size) {
    strncpy(destination.get(host.mem), source.get(host.mem), size);
    return destination;
}

EXPORT(int, strncpy_s) {
    return unimplemented(export_name);
}

EXPORT(int, strnlen_s) {
    return unimplemented(export_name);
}

EXPORT(int, strpbrk) {
    return unimplemented(export_name);
}

EXPORT(Ptr<char>, strrchr, Ptr<char> str, char ch) {
    Ptr<char> res = Ptr<char>();
    char *_str = str.get(host.mem);
    for (int i = strlen(_str) - 1; i >= 0; i--) {
        const char ch1 = _str[i];
        if (ch1 == ch) {
            res = str + i * sizeof(char);
            break;
        }
    }

    return res;
}

EXPORT(int, strspn) {
    return unimplemented(export_name);
}

EXPORT(int, strstr) {
    return unimplemented(export_name);
}

EXPORT(int, strtod) {
    return unimplemented(export_name);
}

EXPORT(int, strtof) {
    return unimplemented(export_name);
}

EXPORT(int, strtoimax) {
    return unimplemented(export_name);
}

EXPORT(int, strtok) {
    return unimplemented(export_name);
}

EXPORT(int, strtok_r) {
    return unimplemented(export_name);
}

EXPORT(int, strtok_s) {
    return unimplemented(export_name);
}

EXPORT(int, strtol) {
    return unimplemented(export_name);
}

EXPORT(int, strtold) {
    return unimplemented(export_name);
}

EXPORT(int, strtoll) {
    return unimplemented(export_name);
}

EXPORT(int, strtoul) {
    return unimplemented(export_name);
}

EXPORT(int, strtoull) {
    return unimplemented(export_name);
}

EXPORT(int, strtoumax) {
    return unimplemented(export_name);
}

EXPORT(int, strxfrm) {
    return unimplemented(export_name);
}

EXPORT(int, swprintf) {
    return unimplemented(export_name);
}

EXPORT(int, swprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, swscanf) {
    return unimplemented(export_name);
}

EXPORT(int, swscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, time) {
    return unimplemented(export_name);
}

EXPORT(int, tolower) {
    return unimplemented(export_name);
}

EXPORT(int, toupper) {
    return unimplemented(export_name);
}

EXPORT(int, towctrans) {
    return unimplemented(export_name);
}

EXPORT(int, towlower) {
    return unimplemented(export_name);
}

EXPORT(int, towupper) {
    return unimplemented(export_name);
}

EXPORT(int, ungetc) {
    return unimplemented(export_name);
}

EXPORT(int, ungetwc) {
    return unimplemented(export_name);
}

EXPORT(int, vfprintf) {
    return unimplemented(export_name);
}

EXPORT(int, vfprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vfscanf) {
    return unimplemented(export_name);
}

EXPORT(int, vfscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vfwprintf) {
    return unimplemented(export_name);
}

EXPORT(int, vfwprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vfwscanf) {
    return unimplemented(export_name);
}

EXPORT(int, vfwscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vprintf) {
    return unimplemented(export_name);
}

EXPORT(int, vprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vscanf) {
    return unimplemented(export_name);
}

EXPORT(int, vscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vsnprintf, char *s, size_t n, const char *format, va_list arg) {
    return snprintf(s, n, format, "");
}

EXPORT(int, vsnprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vsnwprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vsprintf) {
    return unimplemented(export_name);
}

EXPORT(int, vsprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vsscanf) {
    return unimplemented(export_name);
}

EXPORT(int, vsscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vswprintf) {
    return unimplemented(export_name);
}

EXPORT(int, vswprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vswscanf) {
    return unimplemented(export_name);
}

EXPORT(int, vswscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vwprintf) {
    return unimplemented(export_name);
}

EXPORT(int, vwprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, vwscanf) {
    return unimplemented(export_name);
}

EXPORT(int, vwscanf_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcrtomb) {
    return unimplemented(export_name);
}

EXPORT(int, wcrtomb_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcscat) {
    return unimplemented(export_name);
}

EXPORT(int, wcscat_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcschr) {
    return unimplemented(export_name);
}

EXPORT(int, wcscmp) {
    return unimplemented(export_name);
}

EXPORT(int, wcscoll) {
    return unimplemented(export_name);
}

EXPORT(int, wcscpy) {
    return unimplemented(export_name);
}

EXPORT(int, wcscpy_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcscspn) {
    return unimplemented(export_name);
}

EXPORT(int, wcsftime) {
    return unimplemented(export_name);
}

EXPORT(int, wcslen) {
    return unimplemented(export_name);
}

EXPORT(int, wcsncat) {
    return unimplemented(export_name);
}

EXPORT(int, wcsncat_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcsncmp) {
    return unimplemented(export_name);
}

EXPORT(int, wcsncpy) {
    return unimplemented(export_name);
}

EXPORT(int, wcsncpy_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcsnlen_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcspbrk) {
    return unimplemented(export_name);
}

EXPORT(int, wcsrchr) {
    return unimplemented(export_name);
}

EXPORT(int, wcsrtombs) {
    return unimplemented(export_name);
}

EXPORT(int, wcsrtombs_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcsspn) {
    return unimplemented(export_name);
}

EXPORT(int, wcsstr) {
    return unimplemented(export_name);
}

EXPORT(int, wcstod) {
    return unimplemented(export_name);
}

EXPORT(int, wcstof) {
    return unimplemented(export_name);
}

EXPORT(int, wcstoimax) {
    return unimplemented(export_name);
}

EXPORT(int, wcstok) {
    return unimplemented(export_name);
}

EXPORT(int, wcstok_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcstol) {
    return unimplemented(export_name);
}

EXPORT(int, wcstold) {
    return unimplemented(export_name);
}

EXPORT(int, wcstoll) {
    return unimplemented(export_name);
}

EXPORT(int, wcstombs) {
    return unimplemented(export_name);
}

EXPORT(int, wcstombs_s) {
    return unimplemented(export_name);
}

EXPORT(int, wcstoul) {
    return unimplemented(export_name);
}

EXPORT(int, wcstoull) {
    return unimplemented(export_name);
}

EXPORT(int, wcstoumax) {
    return unimplemented(export_name);
}

EXPORT(int, wcsxfrm) {
    return unimplemented(export_name);
}

EXPORT(int, wctob) {
    return unimplemented(export_name);
}

EXPORT(int, wctomb) {
    return unimplemented(export_name);
}

EXPORT(int, wctomb_s) {
    return unimplemented(export_name);
}

EXPORT(int, wctrans) {
    return unimplemented(export_name);
}

EXPORT(int, wctype) {
    return unimplemented(export_name);
}

EXPORT(int, wmemchr) {
    return unimplemented(export_name);
}

EXPORT(int, wmemcmp) {
    return unimplemented(export_name);
}

EXPORT(int, wmemcpy) {
    return unimplemented(export_name);
}

EXPORT(int, wmemcpy_s) {
    return unimplemented(export_name);
}

EXPORT(int, wmemmove) {
    return unimplemented(export_name);
}

EXPORT(int, wmemmove_s) {
    return unimplemented(export_name);
}

EXPORT(int, wmemset) {
    return unimplemented(export_name);
}

EXPORT(int, wprintf) {
    return unimplemented(export_name);
}

EXPORT(int, wprintf_s) {
    return unimplemented(export_name);
}

EXPORT(int, wscanf) {
    return unimplemented(export_name);
}

EXPORT(int, wscanf_s) {
    return unimplemented(export_name);
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
