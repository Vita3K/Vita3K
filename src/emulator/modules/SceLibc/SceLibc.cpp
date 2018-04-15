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
    return unimplemented("_Assert");
}

EXPORT(int, _Btowc) {
    return unimplemented("_Btowc");
}

EXPORT(int, _Ctype) {
    return unimplemented("_Ctype");
}

EXPORT(int, _Dbl) {
    return unimplemented("_Dbl");
}

EXPORT(int, _Denorm) {
    return unimplemented("_Denorm");
}

EXPORT(int, _Exit) {
    return unimplemented("_Exit");
}

EXPORT(int, _FCbuild) {
    return unimplemented("_FCbuild");
}

EXPORT(int, _FDenorm) {
    return unimplemented("_FDenorm");
}

EXPORT(int, _FInf) {
    return unimplemented("_FInf");
}

EXPORT(int, _FNan) {
    return unimplemented("_FNan");
}

EXPORT(int, _FSnan) {
    return unimplemented("_FSnan");
}

EXPORT(int, _Files) {
    return unimplemented("_Files");
}

EXPORT(int, _Flt) {
    return unimplemented("_Flt");
}

EXPORT(int, _Fltrounds) {
    return unimplemented("_Fltrounds");
}

EXPORT(int, _Inf) {
    return unimplemented("_Inf");
}

EXPORT(int, _Iswctype) {
    return unimplemented("_Iswctype");
}

EXPORT(int, _LDenorm) {
    return unimplemented("_LDenorm");
}

EXPORT(int, _LInf) {
    return unimplemented("_LInf");
}

EXPORT(int, _LNan) {
    return unimplemented("_LNan");
}

EXPORT(int, _LSnan) {
    return unimplemented("_LSnan");
}

EXPORT(int, _Ldbl) {
    return unimplemented("_Ldbl");
}

EXPORT(int, _Lockfilelock) {
    return unimplemented("_Lockfilelock");
}

EXPORT(int, _Locksyslock) {
    return unimplemented("_Locksyslock");
}

EXPORT(int, _Mbtowc) {
    return unimplemented("_Mbtowc");
}

EXPORT(int, _Nan) {
    return unimplemented("_Nan");
}

EXPORT(int, _PJP_C_Copyright) {
    return unimplemented("_PJP_C_Copyright");
}

EXPORT(int, _SCE_Assert) {
    return unimplemented("_SCE_Assert");
}

EXPORT(int, _Snan) {
    return unimplemented("_Snan");
}

EXPORT(int, _Stderr) {
    return unimplemented("_Stderr");
}

EXPORT(int, _Stdin) {
    return unimplemented("_Stdin");
}

EXPORT(int, _Stdout) {
    return unimplemented("_Stdout");
}

EXPORT(int, _Stod) {
    return unimplemented("_Stod");
}

EXPORT(int, _Stodx) {
    return unimplemented("_Stodx");
}

EXPORT(int, _Stof) {
    return unimplemented("_Stof");
}

EXPORT(int, _Stofx) {
    return unimplemented("_Stofx");
}

EXPORT(int, _Stold) {
    return unimplemented("_Stold");
}

EXPORT(int, _Stoldx) {
    return unimplemented("_Stoldx");
}

EXPORT(int, _Stoll) {
    return unimplemented("_Stoll");
}

EXPORT(int, _Stollx) {
    return unimplemented("_Stollx");
}

EXPORT(int, _Stolx) {
    return unimplemented("_Stolx");
}

EXPORT(int, _Stoul) {
    return unimplemented("_Stoul");
}

EXPORT(int, _Stoull) {
    return unimplemented("_Stoull");
}

EXPORT(int, _Stoullx) {
    return unimplemented("_Stoullx");
}

EXPORT(int, _Stoulx) {
    return unimplemented("_Stoulx");
}

EXPORT(int, _Tolotab) {
    return unimplemented("_Tolotab");
}

EXPORT(int, _Touptab) {
    return unimplemented("_Touptab");
}

EXPORT(int, _Towctrans) {
    return unimplemented("_Towctrans");
}

EXPORT(int, _Unlockfilelock) {
    return unimplemented("_Unlockfilelock");
}

EXPORT(int, _Unlocksyslock) {
    return unimplemented("_Unlocksyslock");
}

EXPORT(int, _WStod) {
    return unimplemented("_WStod");
}

EXPORT(int, _WStof) {
    return unimplemented("_WStof");
}

EXPORT(int, _WStold) {
    return unimplemented("_WStold");
}

EXPORT(int, _WStoul) {
    return unimplemented("_WStoul");
}

EXPORT(int, _Wctob) {
    return unimplemented("_Wctob");
}

EXPORT(int, _Wctomb) {
    return unimplemented("_Wctomb");
}

EXPORT(int, __aeabi_atexit) {
    return unimplemented("__aeabi_atexit");
}

EXPORT(int, __at_quick_exit) {
    return unimplemented("__at_quick_exit");
}

EXPORT(int, __cxa_atexit) {
    return unimplemented("__cxa_atexit");
}

EXPORT(int, __cxa_finalize) {
    return unimplemented("__cxa_finalize");
}

EXPORT(int, __cxa_guard_abort) {
    return unimplemented("__cxa_guard_abort");
}

EXPORT(int, __cxa_guard_acquire) {
    return unimplemented("__cxa_guard_acquire");
}

EXPORT(int, __cxa_guard_release) {
    return unimplemented("__cxa_guard_release");
}

EXPORT(void, __cxa_set_dso_handle_main, Ptr<void> dso) {
    //LOG_WARN("__cxa_set_dso_handle_main(dso=*0x%x)", dso);
    g_dso = dso;
    //return unimplemented("__cxa_set_dso_handle_main");
}

EXPORT(int, __set_exidx_main) {
    return unimplemented("__set_exidx_main");
}

EXPORT(int, __tls_get_addr) {
    return unimplemented("__tls_get_addr");
}

EXPORT(int, _sceLdTlsRegisterModuleInfo) {
    return unimplemented("_sceLdTlsRegisterModuleInfo");
}

EXPORT(int, _sceLdTlsUnregisterModuleInfo) {
    return unimplemented("_sceLdTlsUnregisterModuleInfo");
}

EXPORT(int, _sceLibcErrnoLoc) {
    return unimplemented("_sceLibcErrnoLoc");
}

EXPORT(int, abort) {
    return unimplemented("abort");
}

EXPORT(int, abort_handler_s) {
    return unimplemented("abort_handler_s");
}

EXPORT(int, abs) {
    return unimplemented("abs");
}

EXPORT(int, asctime) {
    return unimplemented("asctime");
}

EXPORT(int, asctime_s) {
    return unimplemented("asctime_s");
}

EXPORT(int, atof) {
    return unimplemented("atof");
}

EXPORT(int, atoff) {
    return unimplemented("atoff");
}

EXPORT(int, atoi) {
    return unimplemented("atoi");
}

EXPORT(int, atol) {
    return unimplemented("atol");
}

EXPORT(int, atoll) {
    return unimplemented("atoll");
}

EXPORT(int, bsearch) {
    return unimplemented("bsearch");
}

EXPORT(int, bsearch_s) {
    return unimplemented("bsearch_s");
}

EXPORT(int, btowc) {
    return unimplemented("btowc");
}

EXPORT(int, c16rtomb) {
    return unimplemented("c16rtomb");
}

EXPORT(int, c32rtomb) {
    return unimplemented("c32rtomb");
}

EXPORT(int, calloc) {
    return unimplemented("calloc");
}

EXPORT(int, clearerr) {
    return unimplemented("clearerr");
}

EXPORT(int, clock) {
    return unimplemented("clock");
}

EXPORT(int, ctime) {
    return unimplemented("ctime");
}

EXPORT(int, ctime_s) {
    return unimplemented("ctime_s");
}

EXPORT(int, difftime) {
    return unimplemented("difftime");
}

EXPORT(int, div) {
    return unimplemented("div");
}

EXPORT(int, exit) {
    return unimplemented("exit");
}

EXPORT(void, fclose, SceUID file) {
    close_file(host.io, file);
}

EXPORT(int, fdopen) {
    return unimplemented("fdopen");
}

EXPORT(int, feof) {
    return unimplemented("feof");
}

EXPORT(int, ferror) {
    return unimplemented("ferror");
}

EXPORT(int, fflush) {
    return unimplemented("fflush");
}

EXPORT(int, fgetc) {
    return unimplemented("fgetc");
}

EXPORT(int, fgetpos) {
    return unimplemented("fgetpos");
}

EXPORT(int, fgets) {
    return unimplemented("fgets");
}

EXPORT(int, fgetwc) {
    return unimplemented("fgetwc");
}

EXPORT(int, fgetws) {
    return unimplemented("fgetws");
}

EXPORT(int, fileno) {
    return unimplemented("fileno");
}

EXPORT(int, fopen, const char *filename, const char *mode) {
    return open_file(host.io, filename, SCE_O_RDONLY, host.pref_path.c_str());
}

EXPORT(int, fopen_s) {
    return unimplemented("fopen_s");
}

EXPORT(int, fprintf) {
    return unimplemented("fprintf");
}

EXPORT(int, fprintf_s) {
    return unimplemented("fprintf_s");
}

EXPORT(int, fputc) {
    return unimplemented("fputc");
}

EXPORT(int, fputs) {
    return unimplemented("fputs");
}

EXPORT(int, fputwc) {
    return unimplemented("fputwc");
}

EXPORT(int, fputws) {
    return unimplemented("fputws");
}

EXPORT(int, fread) {
    return unimplemented("fread");
}

EXPORT(void, free, Address mem) {
    free(host.mem, mem);
}

EXPORT(int, freopen) {
    return unimplemented("freopen");
}

EXPORT(int, freopen_s) {
    return unimplemented("freopen_s");
}

EXPORT(int, fscanf) {
    return unimplemented("fscanf");
}

EXPORT(int, fscanf_s) {
    return unimplemented("fscanf_s");
}

EXPORT(int, fseek) {
    return unimplemented("fseek");
}

EXPORT(int, fsetpos) {
    return unimplemented("fsetpos");
}

EXPORT(int, ftell) {
    return unimplemented("ftell");
}

EXPORT(int, fwide) {
    return unimplemented("fwide");
}

EXPORT(int, fwprintf) {
    return unimplemented("fwprintf");
}

EXPORT(int, fwprintf_s) {
    return unimplemented("fwprintf_s");
}

EXPORT(int, fwrite) {
    return unimplemented("fwrite");
}

EXPORT(int, fwscanf) {
    return unimplemented("fwscanf");
}

EXPORT(int, fwscanf_s) {
    return unimplemented("fwscanf_s");
}

EXPORT(int, getc) {
    return unimplemented("getc");
}

EXPORT(int, getchar) {
    return unimplemented("getchar");
}

EXPORT(int, gets) {
    return unimplemented("gets");
}

EXPORT(int, gets_s) {
    return unimplemented("gets_s");
}

EXPORT(int, getwc) {
    return unimplemented("getwc");
}

EXPORT(int, getwchar) {
    return unimplemented("getwchar");
}

EXPORT(int, gmtime) {
    return unimplemented("gmtime");
}

EXPORT(int, gmtime_s) {
    return unimplemented("gmtime_s");
}

EXPORT(int, ignore_handler_s) {
    return unimplemented("ignore_handler_s");
}

EXPORT(int, imaxabs) {
    return unimplemented("imaxabs");
}

EXPORT(int, imaxdiv) {
    return unimplemented("imaxdiv");
}

EXPORT(int, isalnum) {
    return unimplemented("isalnum");
}

EXPORT(int, isalpha) {
    return unimplemented("isalpha");
}

EXPORT(int, isblank) {
    return unimplemented("isblank");
}

EXPORT(int, iscntrl) {
    return unimplemented("iscntrl");
}

EXPORT(int, isdigit) {
    return unimplemented("isdigit");
}

EXPORT(int, isgraph) {
    return unimplemented("isgraph");
}

EXPORT(int, islower) {
    return unimplemented("islower");
}

EXPORT(int, isprint) {
    return unimplemented("isprint");
}

EXPORT(int, ispunct) {
    return unimplemented("ispunct");
}

EXPORT(int, isspace) {
    return unimplemented("isspace");
}

EXPORT(int, isupper) {
    return unimplemented("isupper");
}

EXPORT(int, iswalnum) {
    return unimplemented("iswalnum");
}

EXPORT(int, iswalpha) {
    return unimplemented("iswalpha");
}

EXPORT(int, iswblank) {
    return unimplemented("iswblank");
}

EXPORT(int, iswcntrl) {
    return unimplemented("iswcntrl");
}

EXPORT(int, iswctype) {
    return unimplemented("iswctype");
}

EXPORT(int, iswdigit) {
    return unimplemented("iswdigit");
}

EXPORT(int, iswgraph) {
    return unimplemented("iswgraph");
}

EXPORT(int, iswlower) {
    return unimplemented("iswlower");
}

EXPORT(int, iswprint) {
    return unimplemented("iswprint");
}

EXPORT(int, iswpunct) {
    return unimplemented("iswpunct");
}

EXPORT(int, iswspace) {
    return unimplemented("iswspace");
}

EXPORT(int, iswupper) {
    return unimplemented("iswupper");
}

EXPORT(int, iswxdigit) {
    return unimplemented("iswxdigit");
}

EXPORT(int, isxdigit) {
    return unimplemented("isxdigit");
}

EXPORT(int, labs) {
    return unimplemented("labs");
}

EXPORT(int, ldiv) {
    return unimplemented("ldiv");
}

EXPORT(int, llabs) {
    return unimplemented("llabs");
}

EXPORT(int, lldiv) {
    return unimplemented("lldiv");
}

EXPORT(int, localtime) {
    return unimplemented("localtime");
}

EXPORT(int, localtime_s) {
    return unimplemented("localtime_s");
}

EXPORT(int, longjmp) {
    return unimplemented("longjmp");
}

EXPORT(int, malloc, SceSize size) {
    return alloc(host.mem, size, ""); //return unimplemented("malloc");
}

EXPORT(int, malloc_stats) {
    return unimplemented("malloc_stats");
}

EXPORT(int, malloc_stats_fast) {
    return unimplemented("malloc_stats_fast");
}

EXPORT(int, malloc_usable_size) {
    return unimplemented("malloc_usable_size");
}

EXPORT(int, mblen) {
    return unimplemented("mblen");
}

EXPORT(int, mbrlen) {
    return unimplemented("mbrlen");
}

EXPORT(int, mbrtoc16) {
    return unimplemented("mbrtoc16");
}

EXPORT(int, mbrtoc32) {
    return unimplemented("mbrtoc32");
}

EXPORT(int, mbrtowc) {
    return unimplemented("mbrtowc");
}

EXPORT(int, mbsinit) {
    return unimplemented("mbsinit");
}

EXPORT(int, mbsrtowcs) {
    return unimplemented("mbsrtowcs");
}

EXPORT(int, mbsrtowcs_s) {
    return unimplemented("mbsrtowcs_s");
}

EXPORT(int, mbstowcs) {
    return unimplemented("mbstowcs");
}

EXPORT(int, mbstowcs_s) {
    return unimplemented("mbstowcs_s");
}

EXPORT(int, mbtowc) {
    return unimplemented("mbtowc");
}

EXPORT(int, memalign) {
    return unimplemented("memalign");
}

EXPORT(int, memchr) {
    return unimplemented("memchr");
}

EXPORT(int, memcmp) {
    return unimplemented("memcmp");
}

EXPORT(void, memcpy, void *destination, const void *source, size_t num) {
    memcpy(destination, source, num);
}

EXPORT(int, memcpy_s) {
    return unimplemented("memcpy_s");
}

EXPORT(int, memmove) {
    return unimplemented("memmove");
}

EXPORT(int, memmove_s) {
    return unimplemented("memmove_s");
}

EXPORT(void, memset, Ptr<void> str, int c, size_t n) {
    memset(str.get(host.mem), c, n);
}

EXPORT(int, mktime) {
    return unimplemented("mktime");
}

EXPORT(int, mspace_calloc) {
    return unimplemented("mspace_calloc");
}

EXPORT(int, mspace_create) {
    return unimplemented("mspace_create");
}

EXPORT(int, mspace_create_with_flag) {
    return unimplemented("mspace_create_with_flag");
}

EXPORT(int, mspace_destroy) {
    return unimplemented("mspace_destroy");
}

EXPORT(int, mspace_free) {
    return unimplemented("mspace_free");
}

EXPORT(int, mspace_is_heap_empty) {
    return unimplemented("mspace_is_heap_empty");
}

EXPORT(int, mspace_malloc) {
    return unimplemented("mspace_malloc");
}

EXPORT(int, mspace_malloc_stats) {
    return unimplemented("mspace_malloc_stats");
}

EXPORT(int, mspace_malloc_stats_fast) {
    return unimplemented("mspace_malloc_stats_fast");
}

EXPORT(int, mspace_malloc_usable_size) {
    return unimplemented("mspace_malloc_usable_size");
}

EXPORT(int, mspace_memalign) {
    return unimplemented("mspace_memalign");
}

EXPORT(int, mspace_realloc) {
    return unimplemented("mspace_realloc");
}

EXPORT(int, mspace_reallocalign) {
    return unimplemented("mspace_reallocalign");
}

EXPORT(int, perror) {
    return unimplemented("perror");
}

EXPORT(int, printf, char *fmt, va_list va_args) {
    LOG_INFO("{}", fmt);
    return 0;
}

EXPORT(int, printf_s) {
    return unimplemented("printf_s");
}

EXPORT(int, putc) {
    return unimplemented("putc");
}

EXPORT(int, putchar) {
    return unimplemented("putchar");
}

EXPORT(int, puts) {
    return unimplemented("puts");
}

EXPORT(int, putwc) {
    return unimplemented("putwc");
}

EXPORT(int, putwchar) {
    return unimplemented("putwchar");
}

EXPORT(int, qsort) {
    return unimplemented("qsort");
}

EXPORT(int, qsort_s) {
    return unimplemented("qsort_s");
}

EXPORT(int, quick_exit) {
    return unimplemented("quick_exit");
}

EXPORT(int, rand) {
    return unimplemented("rand");
}

EXPORT(int, rand_r) {
    return unimplemented("rand_r");
}

EXPORT(int, realloc) {
    return unimplemented("realloc");
}

EXPORT(int, reallocalign) {
    return unimplemented("reallocalign");
}

EXPORT(int, remove) {
    return unimplemented("remove");
}

EXPORT(int, rename) {
    return unimplemented("rename");
}

EXPORT(int, rewind) {
    return unimplemented("rewind");
}

EXPORT(int, scanf) {
    return unimplemented("scanf");
}

EXPORT(int, scanf_s) {
    return unimplemented("scanf_s");
}

EXPORT(int, sceLibcFopenWithFD) {
    return unimplemented("sceLibcFopenWithFD");
}

EXPORT(int, sceLibcFopenWithFH) {
    return unimplemented("sceLibcFopenWithFH");
}

EXPORT(int, sceLibcGetFD) {
    return unimplemented("sceLibcGetFD");
}

EXPORT(int, sceLibcGetFH) {
    return unimplemented("sceLibcGetFH");
}

EXPORT(int, sceLibcSetHeapInitError) {
    return unimplemented("sceLibcSetHeapInitError");
}

EXPORT(int, set_constraint_handler_s) {
    return unimplemented("set_constraint_handler_s");
}

EXPORT(int, setbuf) {
    return unimplemented("setbuf");
}

EXPORT(int, setjmp) {
    return unimplemented("setjmp");
}

EXPORT(int, setvbuf, FILE *stream, char *buffer, int mode, size_t size) {
    return unimplemented("setvbuf");
}

EXPORT(int, snprintf) {
    return unimplemented("snprintf");
}

EXPORT(int, snprintf_s) {
    return unimplemented("snprintf_s");
}

EXPORT(int, snwprintf_s) {
    return unimplemented("snwprintf_s");
}

EXPORT(int, sprintf) {
    return unimplemented("sprintf");
}

EXPORT(int, sprintf_s) {
    return unimplemented("sprintf_s");
}

EXPORT(int, srand) {
    return unimplemented("srand");
}

EXPORT(int, sscanf) {
    return unimplemented("sscanf");
}

EXPORT(int, sscanf_s) {
    return unimplemented("sscanf_s");
}

EXPORT(int, strcasecmp) {
    return unimplemented("strcasecmp");
}

EXPORT(Ptr<char>, strcat, Ptr<char> destination, Ptr<char> source) {
    strcat(destination.get(host.mem), source.get(host.mem));
    return destination;
}

EXPORT(int, strcat_s) {
    return unimplemented("strcat_s");
}

EXPORT(int, strchr) {
    return unimplemented("strchr");
}

EXPORT(int, strcmp) {
    return unimplemented("strcmp");
}

EXPORT(int, strcoll) {
    return unimplemented("strcoll");
}

EXPORT(Ptr<char>, strcpy, Ptr<char> destination, Ptr<char> source) {
    strcpy(destination.get(host.mem), source.get(host.mem));
    return destination;
}

EXPORT(int, strcpy_s) {
    return unimplemented("strcpy_s");
}

EXPORT(int, strcspn) {
    return unimplemented("strcspn");
}

EXPORT(int, strdup) {
    return unimplemented("strdup");
}

EXPORT(int, strerror) {
    return unimplemented("strerror");
}

EXPORT(int, strerror_s) {
    return unimplemented("strerror_s");
}

EXPORT(int, strerrorlen_s) {
    return unimplemented("strerrorlen_s");
}

EXPORT(int, strftime) {
    return unimplemented("strftime");
}

EXPORT(int, strlen, char *str) {
    return strlen(str);
}

EXPORT(int, strncasecmp) {
    return unimplemented("strncasecmp");
}

EXPORT(int, strncat) {
    return unimplemented("strncat");
}

EXPORT(int, strncat_s) {
    return unimplemented("strncat_s");
}

EXPORT(int, strncmp, const char *str1, const char *str2, size_t num) {
    return strncmp(str1, str2, num);
}

EXPORT(Ptr<char>, strncpy, Ptr<char> destination, Ptr<char> source, SceSize size) {
    strncpy(destination.get(host.mem), source.get(host.mem), size);
    return destination;
}

EXPORT(int, strncpy_s) {
    return unimplemented("strncpy_s");
}

EXPORT(int, strnlen_s) {
    return unimplemented("strnlen_s");
}

EXPORT(int, strpbrk) {
    return unimplemented("strpbrk");
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
    return unimplemented("strspn");
}

EXPORT(int, strstr) {
    return unimplemented("strstr");
}

EXPORT(int, strtod) {
    return unimplemented("strtod");
}

EXPORT(int, strtof) {
    return unimplemented("strtof");
}

EXPORT(int, strtoimax) {
    return unimplemented("strtoimax");
}

EXPORT(int, strtok) {
    return unimplemented("strtok");
}

EXPORT(int, strtok_r) {
    return unimplemented("strtok_r");
}

EXPORT(int, strtok_s) {
    return unimplemented("strtok_s");
}

EXPORT(int, strtol) {
    return unimplemented("strtol");
}

EXPORT(int, strtold) {
    return unimplemented("strtold");
}

EXPORT(int, strtoll) {
    return unimplemented("strtoll");
}

EXPORT(int, strtoul) {
    return unimplemented("strtoul");
}

EXPORT(int, strtoull) {
    return unimplemented("strtoull");
}

EXPORT(int, strtoumax) {
    return unimplemented("strtoumax");
}

EXPORT(int, strxfrm) {
    return unimplemented("strxfrm");
}

EXPORT(int, swprintf) {
    return unimplemented("swprintf");
}

EXPORT(int, swprintf_s) {
    return unimplemented("swprintf_s");
}

EXPORT(int, swscanf) {
    return unimplemented("swscanf");
}

EXPORT(int, swscanf_s) {
    return unimplemented("swscanf_s");
}

EXPORT(int, time) {
    return unimplemented("time");
}

EXPORT(int, tolower) {
    return unimplemented("tolower");
}

EXPORT(int, toupper) {
    return unimplemented("toupper");
}

EXPORT(int, towctrans) {
    return unimplemented("towctrans");
}

EXPORT(int, towlower) {
    return unimplemented("towlower");
}

EXPORT(int, towupper) {
    return unimplemented("towupper");
}

EXPORT(int, ungetc) {
    return unimplemented("ungetc");
}

EXPORT(int, ungetwc) {
    return unimplemented("ungetwc");
}

EXPORT(int, vfprintf) {
    return unimplemented("vfprintf");
}

EXPORT(int, vfprintf_s) {
    return unimplemented("vfprintf_s");
}

EXPORT(int, vfscanf) {
    return unimplemented("vfscanf");
}

EXPORT(int, vfscanf_s) {
    return unimplemented("vfscanf_s");
}

EXPORT(int, vfwprintf) {
    return unimplemented("vfwprintf");
}

EXPORT(int, vfwprintf_s) {
    return unimplemented("vfwprintf_s");
}

EXPORT(int, vfwscanf) {
    return unimplemented("vfwscanf");
}

EXPORT(int, vfwscanf_s) {
    return unimplemented("vfwscanf_s");
}

EXPORT(int, vprintf) {
    return unimplemented("vprintf");
}

EXPORT(int, vprintf_s) {
    return unimplemented("vprintf_s");
}

EXPORT(int, vscanf) {
    return unimplemented("vscanf");
}

EXPORT(int, vscanf_s) {
    return unimplemented("vscanf_s");
}

EXPORT(int, vsnprintf, char *s, size_t n, const char *format, va_list arg) {
    return snprintf(s, n, format, "");
}

EXPORT(int, vsnprintf_s) {
    return unimplemented("vsnprintf_s");
}

EXPORT(int, vsnwprintf_s) {
    return unimplemented("vsnwprintf_s");
}

EXPORT(int, vsprintf) {
    return unimplemented("vsprintf");
}

EXPORT(int, vsprintf_s) {
    return unimplemented("vsprintf_s");
}

EXPORT(int, vsscanf) {
    return unimplemented("vsscanf");
}

EXPORT(int, vsscanf_s) {
    return unimplemented("vsscanf_s");
}

EXPORT(int, vswprintf) {
    return unimplemented("vswprintf");
}

EXPORT(int, vswprintf_s) {
    return unimplemented("vswprintf_s");
}

EXPORT(int, vswscanf) {
    return unimplemented("vswscanf");
}

EXPORT(int, vswscanf_s) {
    return unimplemented("vswscanf_s");
}

EXPORT(int, vwprintf) {
    return unimplemented("vwprintf");
}

EXPORT(int, vwprintf_s) {
    return unimplemented("vwprintf_s");
}

EXPORT(int, vwscanf) {
    return unimplemented("vwscanf");
}

EXPORT(int, vwscanf_s) {
    return unimplemented("vwscanf_s");
}

EXPORT(int, wcrtomb) {
    return unimplemented("wcrtomb");
}

EXPORT(int, wcrtomb_s) {
    return unimplemented("wcrtomb_s");
}

EXPORT(int, wcscat) {
    return unimplemented("wcscat");
}

EXPORT(int, wcscat_s) {
    return unimplemented("wcscat_s");
}

EXPORT(int, wcschr) {
    return unimplemented("wcschr");
}

EXPORT(int, wcscmp) {
    return unimplemented("wcscmp");
}

EXPORT(int, wcscoll) {
    return unimplemented("wcscoll");
}

EXPORT(int, wcscpy) {
    return unimplemented("wcscpy");
}

EXPORT(int, wcscpy_s) {
    return unimplemented("wcscpy_s");
}

EXPORT(int, wcscspn) {
    return unimplemented("wcscspn");
}

EXPORT(int, wcsftime) {
    return unimplemented("wcsftime");
}

EXPORT(int, wcslen) {
    return unimplemented("wcslen");
}

EXPORT(int, wcsncat) {
    return unimplemented("wcsncat");
}

EXPORT(int, wcsncat_s) {
    return unimplemented("wcsncat_s");
}

EXPORT(int, wcsncmp) {
    return unimplemented("wcsncmp");
}

EXPORT(int, wcsncpy) {
    return unimplemented("wcsncpy");
}

EXPORT(int, wcsncpy_s) {
    return unimplemented("wcsncpy_s");
}

EXPORT(int, wcsnlen_s) {
    return unimplemented("wcsnlen_s");
}

EXPORT(int, wcspbrk) {
    return unimplemented("wcspbrk");
}

EXPORT(int, wcsrchr) {
    return unimplemented("wcsrchr");
}

EXPORT(int, wcsrtombs) {
    return unimplemented("wcsrtombs");
}

EXPORT(int, wcsrtombs_s) {
    return unimplemented("wcsrtombs_s");
}

EXPORT(int, wcsspn) {
    return unimplemented("wcsspn");
}

EXPORT(int, wcsstr) {
    return unimplemented("wcsstr");
}

EXPORT(int, wcstod) {
    return unimplemented("wcstod");
}

EXPORT(int, wcstof) {
    return unimplemented("wcstof");
}

EXPORT(int, wcstoimax) {
    return unimplemented("wcstoimax");
}

EXPORT(int, wcstok) {
    return unimplemented("wcstok");
}

EXPORT(int, wcstok_s) {
    return unimplemented("wcstok_s");
}

EXPORT(int, wcstol) {
    return unimplemented("wcstol");
}

EXPORT(int, wcstold) {
    return unimplemented("wcstold");
}

EXPORT(int, wcstoll) {
    return unimplemented("wcstoll");
}

EXPORT(int, wcstombs) {
    return unimplemented("wcstombs");
}

EXPORT(int, wcstombs_s) {
    return unimplemented("wcstombs_s");
}

EXPORT(int, wcstoul) {
    return unimplemented("wcstoul");
}

EXPORT(int, wcstoull) {
    return unimplemented("wcstoull");
}

EXPORT(int, wcstoumax) {
    return unimplemented("wcstoumax");
}

EXPORT(int, wcsxfrm) {
    return unimplemented("wcsxfrm");
}

EXPORT(int, wctob) {
    return unimplemented("wctob");
}

EXPORT(int, wctomb) {
    return unimplemented("wctomb");
}

EXPORT(int, wctomb_s) {
    return unimplemented("wctomb_s");
}

EXPORT(int, wctrans) {
    return unimplemented("wctrans");
}

EXPORT(int, wctype) {
    return unimplemented("wctype");
}

EXPORT(int, wmemchr) {
    return unimplemented("wmemchr");
}

EXPORT(int, wmemcmp) {
    return unimplemented("wmemcmp");
}

EXPORT(int, wmemcpy) {
    return unimplemented("wmemcpy");
}

EXPORT(int, wmemcpy_s) {
    return unimplemented("wmemcpy_s");
}

EXPORT(int, wmemmove) {
    return unimplemented("wmemmove");
}

EXPORT(int, wmemmove_s) {
    return unimplemented("wmemmove_s");
}

EXPORT(int, wmemset) {
    return unimplemented("wmemset");
}

EXPORT(int, wprintf) {
    return unimplemented("wprintf");
}

EXPORT(int, wprintf_s) {
    return unimplemented("wprintf_s");
}

EXPORT(int, wscanf) {
    return unimplemented("wscanf");
}

EXPORT(int, wscanf_s) {
    return unimplemented("wscanf_s");
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
