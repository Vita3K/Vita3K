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

#include <io/functions.h>
#include <kernel/state.h>
#include <util/lock_and_find.h>
#include <util/log.h>
#include <util/tracy.h>

#include <dlmalloc.h>
#include <v3kprintf.h>

TRACY_MODULE_NAME(SceLibc);

static Ptr<void> g_dso;

EXPORT(int, _Assert) {
    TRACY_FUNC(_Assert);
    return UNIMPLEMENTED();
}

EXPORT(int, _Btowc) {
    TRACY_FUNC(_Btowc);
    return UNIMPLEMENTED();
}

EXPORT(int, _Exit) {
    TRACY_FUNC(_Exit);
    return UNIMPLEMENTED();
}

EXPORT(int, _FCbuild) {
    TRACY_FUNC(_FCbuild);
    return UNIMPLEMENTED();
}

EXPORT(int, _Fltrounds) {
    TRACY_FUNC(_Fltrounds);
    return UNIMPLEMENTED();
}

EXPORT(int, _Iswctype) {
    TRACY_FUNC(_Iswctype);
    return UNIMPLEMENTED();
}

EXPORT(int, _Lockfilelock) {
    TRACY_FUNC(_Lockfilelock);
    return UNIMPLEMENTED();
}

EXPORT(int, _Locksyslock) {
    TRACY_FUNC(_Locksyslock);
    return UNIMPLEMENTED();
}

EXPORT(int, _Mbtowc) {
    TRACY_FUNC(_Mbtowc);
    return UNIMPLEMENTED();
}

EXPORT(int, _SCE_Assert) {
    TRACY_FUNC(_SCE_Assert);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stod) {
    TRACY_FUNC(_Stod);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stodx) {
    TRACY_FUNC(_Stodx);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stof) {
    TRACY_FUNC(_Stof);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stofx) {
    TRACY_FUNC(_Stofx);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stold) {
    TRACY_FUNC(_Stold);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoldx) {
    TRACY_FUNC(_Stoldx);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoll) {
    TRACY_FUNC(_Stoll);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stollx) {
    TRACY_FUNC(_Stollx);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stolx) {
    TRACY_FUNC(_Stolx);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoul) {
    TRACY_FUNC(_Stoul);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoull) {
    TRACY_FUNC(_Stoull);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoullx) {
    TRACY_FUNC(_Stoullx);
    return UNIMPLEMENTED();
}

EXPORT(int, _Stoulx) {
    TRACY_FUNC(_Stoulx);
    return UNIMPLEMENTED();
}

EXPORT(int, _Towctrans) {
    TRACY_FUNC(_Towctrans);
    return UNIMPLEMENTED();
}

EXPORT(int, _Unlockfilelock) {
    TRACY_FUNC(_Unlockfilelock);
    return UNIMPLEMENTED();
}

EXPORT(int, _Unlocksyslock) {
    TRACY_FUNC(_Unlocksyslock);
    return UNIMPLEMENTED();
}

EXPORT(int, _WStod) {
    TRACY_FUNC(_WStod);
    return UNIMPLEMENTED();
}

EXPORT(int, _WStof) {
    TRACY_FUNC(_WStof);
    return UNIMPLEMENTED();
}

EXPORT(int, _WStold) {
    TRACY_FUNC(_WStold);
    return UNIMPLEMENTED();
}

EXPORT(int, _WStoul) {
    TRACY_FUNC(_WStoul);
    return UNIMPLEMENTED();
}

EXPORT(int, _Wctob) {
    TRACY_FUNC(_Wctob);
    return UNIMPLEMENTED();
}

EXPORT(int, _Wctomb) {
    TRACY_FUNC(_Wctomb);
    return UNIMPLEMENTED();
}

EXPORT(int, __aeabi_atexit) {
    TRACY_FUNC(__aeabi_atexit);
    return UNIMPLEMENTED();
}

EXPORT(int, __at_quick_exit) {
    TRACY_FUNC(__at_quick_exit);
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_atexit) {
    TRACY_FUNC(__cxa_atexit);
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_finalize) {
    TRACY_FUNC(__cxa_finalize);
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_guard_abort) {
    TRACY_FUNC(__cxa_guard_abort);
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_guard_acquire) {
    TRACY_FUNC(__cxa_guard_acquire);
    return UNIMPLEMENTED();
}

EXPORT(int, __cxa_guard_release) {
    TRACY_FUNC(__cxa_guard_release);
    return UNIMPLEMENTED();
}

EXPORT(void, __cxa_set_dso_handle_main, Ptr<void> dso) {
    TRACY_FUNC(__cxa_set_dso_handle_main, dso);
    // LOG_WARN("__cxa_set_dso_handle_main(dso=*0x%x)", dso);
    g_dso = dso;
    // return UNIMPLEMENTED();
}

EXPORT(int, __set_exidx_main) {
    TRACY_FUNC(__set_exidx_main);
    return UNIMPLEMENTED();
}

EXPORT(int, __tls_get_addr) {
    TRACY_FUNC(__tls_get_addr);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceLdTlsRegisterModuleInfo) {
    TRACY_FUNC(_sceLdTlsRegisterModuleInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceLdTlsUnregisterModuleInfo) {
    TRACY_FUNC(_sceLdTlsUnregisterModuleInfo);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<int>, _sceLibcErrnoLoc) {
    TRACY_FUNC(_sceLibcErrnoLoc);
    // tls key from disasmed source
    auto res = emuenv.kernel.get_thread_tls_addr(emuenv.mem, thread_id, TLS_LIBC_ERRNO);
    return res.cast<int>();
}

EXPORT(int, abort) {
    TRACY_FUNC(abort);
    return UNIMPLEMENTED();
}

EXPORT(int, abort_handler_s) {
    TRACY_FUNC(abort_handler_s);
    return UNIMPLEMENTED();
}

EXPORT(int, abs) {
    TRACY_FUNC(abs);
    return UNIMPLEMENTED();
}

EXPORT(int, asctime) {
    TRACY_FUNC(asctime);
    return UNIMPLEMENTED();
}

EXPORT(int, asctime_s) {
    TRACY_FUNC(asctime_s);
    return UNIMPLEMENTED();
}

EXPORT(int, atof) {
    TRACY_FUNC(atof);
    return UNIMPLEMENTED();
}

EXPORT(int, atoff) {
    TRACY_FUNC(atoff);
    return UNIMPLEMENTED();
}

EXPORT(int, atoi, const char *str) {
    TRACY_FUNC(atoi, str);
    return atoi(str);
}

EXPORT(int, atol) {
    TRACY_FUNC(atol);
    return UNIMPLEMENTED();
}

EXPORT(int, atoll) {
    TRACY_FUNC(atoll);
    return UNIMPLEMENTED();
}

EXPORT(int, bsearch) {
    TRACY_FUNC(bsearch);
    return UNIMPLEMENTED();
}

EXPORT(int, bsearch_s) {
    TRACY_FUNC(bsearch_s);
    return UNIMPLEMENTED();
}

EXPORT(int, btowc) {
    TRACY_FUNC(btowc);
    return UNIMPLEMENTED();
}

EXPORT(int, c16rtomb) {
    TRACY_FUNC(c16rtomb);
    return UNIMPLEMENTED();
}

EXPORT(int, c32rtomb) {
    TRACY_FUNC(c32rtomb);
    return UNIMPLEMENTED();
}

EXPORT(int, calloc) {
    TRACY_FUNC(calloc);
    return UNIMPLEMENTED();
}

EXPORT(int, clearerr) {
    TRACY_FUNC(clearerr);
    return UNIMPLEMENTED();
}

EXPORT(int, clock) {
    TRACY_FUNC(clock);
    return UNIMPLEMENTED();
}

EXPORT(int, ctime) {
    TRACY_FUNC(ctime);
    return UNIMPLEMENTED();
}

EXPORT(int, ctime_s) {
    TRACY_FUNC(ctime_s);
    return UNIMPLEMENTED();
}

EXPORT(int, difftime) {
    TRACY_FUNC(difftime);
    return UNIMPLEMENTED();
}

EXPORT(int, div) {
    TRACY_FUNC(div);
    return UNIMPLEMENTED();
}

EXPORT(int, exit) {
    TRACY_FUNC(exit);
    return UNIMPLEMENTED();
}

EXPORT(void, fclose, SceUID file) {
    TRACY_FUNC(fclose, file);
    close_file(emuenv.io, file, export_name);
}

EXPORT(int, fdopen) {
    TRACY_FUNC(fdopen);
    return UNIMPLEMENTED();
}

EXPORT(int, feof) {
    TRACY_FUNC(feof);
    return UNIMPLEMENTED();
}

EXPORT(int, ferror) {
    TRACY_FUNC(ferror);
    return UNIMPLEMENTED();
}

EXPORT(int, fflush) {
    TRACY_FUNC(fflush);
    return UNIMPLEMENTED();
}

EXPORT(int, fgetc) {
    TRACY_FUNC(fgetc);
    return UNIMPLEMENTED();
}

EXPORT(int, fgetpos) {
    TRACY_FUNC(fgetpos);
    return UNIMPLEMENTED();
}

EXPORT(int, fgets) {
    TRACY_FUNC(fgets);
    return UNIMPLEMENTED();
}

EXPORT(int, fgetwc) {
    TRACY_FUNC(fgetwc);
    return UNIMPLEMENTED();
}

EXPORT(int, fgetws) {
    TRACY_FUNC(fgetws);
    return UNIMPLEMENTED();
}

EXPORT(int, fileno) {
    TRACY_FUNC(fileno);
    return UNIMPLEMENTED();
}

EXPORT(int, fopen, const char *filename, const char *mode) {
    TRACY_FUNC(fopen, filename, mode);
    LOG_WARN_IF(mode[0] != 'r', "fopen({}, {})", filename, *mode);
    return open_file(emuenv.io, filename, SCE_O_RDONLY, emuenv.pref_path, export_name);
}

EXPORT(int, fopen_s) {
    TRACY_FUNC(fopen_s);
    return UNIMPLEMENTED();
}

EXPORT(int, fprintf) {
    TRACY_FUNC(fprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, fprintf_s) {
    TRACY_FUNC(fprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, fputc) {
    TRACY_FUNC(fputc);
    return UNIMPLEMENTED();
}

EXPORT(int, fputs) {
    TRACY_FUNC(fputs);
    return UNIMPLEMENTED();
}

EXPORT(int, fputwc) {
    TRACY_FUNC(fputwc);
    return UNIMPLEMENTED();
}

EXPORT(int, fputws) {
    TRACY_FUNC(fputws);
    return UNIMPLEMENTED();
}

EXPORT(int, fread) {
    TRACY_FUNC(fread);
    return UNIMPLEMENTED();
}

EXPORT(void, free, Address mem) {
    TRACY_FUNC(free, mem);
    free(emuenv.mem, mem);
}

EXPORT(int, freopen) {
    TRACY_FUNC(freopen);
    return UNIMPLEMENTED();
}

EXPORT(int, freopen_s) {
    TRACY_FUNC(freopen_s);
    return UNIMPLEMENTED();
}

EXPORT(int, fscanf) {
    TRACY_FUNC(fscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, fscanf_s) {
    TRACY_FUNC(fscanf_s);
    return UNIMPLEMENTED();
}

#ifdef fseek
#undef fseek
#endif
EXPORT(int, fseek) {
    TRACY_FUNC(fseek);
    return UNIMPLEMENTED();
}

EXPORT(int, fsetpos) {
    TRACY_FUNC(fsetpos);
    return UNIMPLEMENTED();
}

#ifdef ftell
#undef ftell
#endif
EXPORT(int, ftell) {
    TRACY_FUNC(ftell);
    return UNIMPLEMENTED();
}

EXPORT(int, fwide) {
    TRACY_FUNC(fwide);
    return UNIMPLEMENTED();
}

EXPORT(int, fwprintf) {
    TRACY_FUNC(fwprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, fwprintf_s) {
    TRACY_FUNC(fwprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, fwrite) {
    TRACY_FUNC(fwrite);
    return UNIMPLEMENTED();
}

EXPORT(int, fwscanf) {
    TRACY_FUNC(fwscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, fwscanf_s) {
    TRACY_FUNC(fwscanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, getc) {
    TRACY_FUNC(getc);
    return UNIMPLEMENTED();
}

EXPORT(int, getchar) {
    TRACY_FUNC(getchar);
    return UNIMPLEMENTED();
}

EXPORT(int, gets) {
    TRACY_FUNC(gets);
    return UNIMPLEMENTED();
}

EXPORT(int, gets_s) {
    TRACY_FUNC(gets_s);
    return UNIMPLEMENTED();
}

EXPORT(int, getwc) {
    TRACY_FUNC(getwc);
    return UNIMPLEMENTED();
}

EXPORT(int, getwchar) {
    TRACY_FUNC(getwchar);
    return UNIMPLEMENTED();
}

EXPORT(int, gmtime) {
    TRACY_FUNC(gmtime);
    return UNIMPLEMENTED();
}

EXPORT(int, gmtime_s) {
    TRACY_FUNC(gmtime_s);
    return UNIMPLEMENTED();
}

EXPORT(int, ignore_handler_s) {
    TRACY_FUNC(ignore_handler_s);
    return UNIMPLEMENTED();
}

EXPORT(int, imaxabs) {
    TRACY_FUNC(imaxabs);
    return UNIMPLEMENTED();
}

EXPORT(int, imaxdiv) {
    TRACY_FUNC(imaxdiv);
    return UNIMPLEMENTED();
}

EXPORT(int, isalnum) {
    TRACY_FUNC(isalnum);
    return UNIMPLEMENTED();
}

EXPORT(int, isalpha) {
    TRACY_FUNC(isalpha);
    return UNIMPLEMENTED();
}

EXPORT(int, isblank) {
    TRACY_FUNC(isblank);
    return UNIMPLEMENTED();
}

EXPORT(int, iscntrl) {
    TRACY_FUNC(iscntrl);
    return UNIMPLEMENTED();
}

EXPORT(int, isdigit) {
    TRACY_FUNC(isdigit);
    return UNIMPLEMENTED();
}

EXPORT(int, isgraph) {
    TRACY_FUNC(isgraph);
    return UNIMPLEMENTED();
}

EXPORT(int, islower) {
    TRACY_FUNC(islower);
    return UNIMPLEMENTED();
}

EXPORT(int, isprint) {
    TRACY_FUNC(isprint);
    return UNIMPLEMENTED();
}

EXPORT(int, ispunct) {
    TRACY_FUNC(ispunct);
    return UNIMPLEMENTED();
}

EXPORT(int, isspace) {
    TRACY_FUNC(isspace);
    return UNIMPLEMENTED();
}

EXPORT(int, isupper) {
    TRACY_FUNC(isupper);
    return UNIMPLEMENTED();
}

EXPORT(int, iswalnum) {
    TRACY_FUNC(iswalnum);
    return UNIMPLEMENTED();
}

EXPORT(int, iswalpha) {
    TRACY_FUNC(iswalpha);
    return UNIMPLEMENTED();
}

EXPORT(int, iswblank) {
    TRACY_FUNC(iswblank);
    return UNIMPLEMENTED();
}

EXPORT(int, iswcntrl) {
    TRACY_FUNC(iswcntrl);
    return UNIMPLEMENTED();
}

EXPORT(int, iswctype) {
    TRACY_FUNC(iswctype);
    return UNIMPLEMENTED();
}

EXPORT(int, iswdigit) {
    TRACY_FUNC(iswdigit);
    return UNIMPLEMENTED();
}

EXPORT(int, iswgraph) {
    TRACY_FUNC(iswgraph);
    return UNIMPLEMENTED();
}

EXPORT(int, iswlower) {
    TRACY_FUNC(iswlower);
    return UNIMPLEMENTED();
}

EXPORT(int, iswprint) {
    TRACY_FUNC(iswprint);
    return UNIMPLEMENTED();
}

EXPORT(int, iswpunct) {
    TRACY_FUNC(iswpunct);
    return UNIMPLEMENTED();
}

EXPORT(int, iswspace) {
    TRACY_FUNC(iswspace);
    return UNIMPLEMENTED();
}

EXPORT(int, iswupper) {
    TRACY_FUNC(iswupper);
    return UNIMPLEMENTED();
}

EXPORT(int, iswxdigit) {
    TRACY_FUNC(iswxdigit);
    return UNIMPLEMENTED();
}

EXPORT(int, isxdigit) {
    TRACY_FUNC(isxdigit);
    return UNIMPLEMENTED();
}

EXPORT(int, labs) {
    TRACY_FUNC(labs);
    return UNIMPLEMENTED();
}

EXPORT(int, ldiv) {
    TRACY_FUNC(ldiv);
    return UNIMPLEMENTED();
}

EXPORT(int, llabs) {
    TRACY_FUNC(llabs);
    return UNIMPLEMENTED();
}

EXPORT(int, lldiv) {
    TRACY_FUNC(lldiv);
    return UNIMPLEMENTED();
}

EXPORT(int, localtime) {
    TRACY_FUNC(localtime);
    return UNIMPLEMENTED();
}

EXPORT(int, localtime_s) {
    TRACY_FUNC(localtime_s);
    return UNIMPLEMENTED();
}

EXPORT(int, longjmp) {
    TRACY_FUNC(longjmp);
    return UNIMPLEMENTED();
}

EXPORT(int, malloc, SceSize size) {
    TRACY_FUNC(malloc, size);
    return alloc(emuenv.mem, size, __FUNCTION__);
}

EXPORT(int, malloc_stats) {
    TRACY_FUNC(malloc_stats);
    return UNIMPLEMENTED();
}

EXPORT(int, malloc_stats_fast) {
    TRACY_FUNC(malloc_stats_fast);
    return UNIMPLEMENTED();
}

EXPORT(int, malloc_usable_size) {
    TRACY_FUNC(malloc_usable_size);
    return UNIMPLEMENTED();
}

EXPORT(int, mblen) {
    TRACY_FUNC(mblen);
    return UNIMPLEMENTED();
}

EXPORT(int, mbrlen) {
    TRACY_FUNC(mbrlen);
    return UNIMPLEMENTED();
}

EXPORT(int, mbrtoc16) {
    TRACY_FUNC(mbrtoc16);
    return UNIMPLEMENTED();
}

EXPORT(int, mbrtoc32) {
    TRACY_FUNC(mbrtoc32);
    return UNIMPLEMENTED();
}

EXPORT(int, mbrtowc) {
    TRACY_FUNC(mbrtowc);
    return UNIMPLEMENTED();
}

EXPORT(int, mbsinit) {
    TRACY_FUNC(mbsinit);
    return UNIMPLEMENTED();
}

EXPORT(int, mbsrtowcs) {
    TRACY_FUNC(mbsrtowcs);
    return UNIMPLEMENTED();
}

EXPORT(int, mbsrtowcs_s) {
    TRACY_FUNC(mbsrtowcs_s);
    return UNIMPLEMENTED();
}

EXPORT(int, mbstowcs) {
    TRACY_FUNC(mbstowcs);
    return UNIMPLEMENTED();
}

EXPORT(int, mbstowcs_s) {
    TRACY_FUNC(mbstowcs_s);
    return UNIMPLEMENTED();
}

EXPORT(int, mbtowc) {
    TRACY_FUNC(mbtowc);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, memalign, uint32_t alignment, uint32_t size) {
    TRACY_FUNC(memalign, alignment, size);
    Address address = alloc_aligned(emuenv.mem, size, "memalign", alignment);

    return Ptr<void>(address);
}

EXPORT(int, memchr) {
    TRACY_FUNC(memchr);
    return UNIMPLEMENTED();
}

EXPORT(int, memcmp) {
    TRACY_FUNC(memcmp);
    return UNIMPLEMENTED();
}

EXPORT(void, memcpy, void *destination, const void *source, uint32_t num) {
    TRACY_FUNC(memcpy, destination, source, num);
    memcpy(destination, source, num);
}

EXPORT(int, memcpy_s) {
    TRACY_FUNC(memcpy_s);
    return UNIMPLEMENTED();
}

EXPORT(void, memmove, void *destination, const void *source, uint32_t num) {
    TRACY_FUNC(memmove, destination, source, num);
    memmove(destination, source, num);
}

EXPORT(int, memmove_s) {
    TRACY_FUNC(memmove_s);
    return UNIMPLEMENTED();
}

EXPORT(void, memset, Ptr<void> str, int c, uint32_t n) {
    TRACY_FUNC(memset, str, c, n);
    memset(str.get(emuenv.mem), c, n);
}

EXPORT(int, mktime) {
    TRACY_FUNC(mktime);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_calloc) {
    TRACY_FUNC(mspace_calloc);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_create) {
    TRACY_FUNC(mspace_create);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_create_internal) {
    TRACY_FUNC(mspace_create_internal);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_create_with_flag) {
    TRACY_FUNC(mspace_create_with_flag);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_destroy) {
    TRACY_FUNC(mspace_destroy);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_free) {
    TRACY_FUNC(mspace_free);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_is_heap_empty) {
    TRACY_FUNC(mspace_is_heap_empty);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_malloc) {
    TRACY_FUNC(mspace_malloc);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_malloc_stats) {
    TRACY_FUNC(mspace_malloc_stats);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_malloc_stats_fast) {
    TRACY_FUNC(mspace_malloc_stats_fast);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_malloc_usable_size) {
    TRACY_FUNC(mspace_malloc_usable_size);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_memalign) {
    TRACY_FUNC(mspace_memalign);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_realloc) {
    TRACY_FUNC(mspace_realloc);
    return UNIMPLEMENTED();
}

EXPORT(int, mspace_reallocalign) {
    TRACY_FUNC(mspace_reallocalign);
    return UNIMPLEMENTED();
}

EXPORT(int, perror) {
    TRACY_FUNC(perror);
    return UNIMPLEMENTED();
}

EXPORT(int, printf, const char *format, module::vargs args) {
    TRACY_FUNC(printf, format);
    // TODO: add args to tracy func
    std::vector<char> buffer(1024);

    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    const int result = utils::snprintf(buffer.data(), buffer.size(), format, *(thread->cpu), emuenv.mem, args);

    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    LOG_INFO("{}", buffer.data());

    return SCE_KERNEL_OK;
}

EXPORT(int, printf_s) {
    TRACY_FUNC(printf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, putc) {
    TRACY_FUNC(putc);
    return UNIMPLEMENTED();
}

EXPORT(int, putchar) {
    TRACY_FUNC(putchar);
    return UNIMPLEMENTED();
}

EXPORT(int, puts) {
    TRACY_FUNC(puts);
    return UNIMPLEMENTED();
}

EXPORT(int, putwc) {
    TRACY_FUNC(putwc);
    return UNIMPLEMENTED();
}

EXPORT(int, putwchar) {
    TRACY_FUNC(putwchar);
    return UNIMPLEMENTED();
}

EXPORT(int, qsort) {
    TRACY_FUNC(qsort);
    return UNIMPLEMENTED();
}

EXPORT(int, qsort_s) {
    TRACY_FUNC(qsort_s);
    return UNIMPLEMENTED();
}

EXPORT(int, quick_exit) {
    TRACY_FUNC(quick_exit);
    return UNIMPLEMENTED();
}

EXPORT(int, rand) {
    TRACY_FUNC(rand);
    return UNIMPLEMENTED();
}

EXPORT(int, rand_r) {
    TRACY_FUNC(rand_r);
    return UNIMPLEMENTED();
}

EXPORT(int, realloc) {
    TRACY_FUNC(realloc);
    return UNIMPLEMENTED();
}

EXPORT(int, reallocalign) {
    TRACY_FUNC(reallocalign);
    return UNIMPLEMENTED();
}

EXPORT(int, remove) {
    TRACY_FUNC(remove);
    return UNIMPLEMENTED();
}

EXPORT(int, rename) {
    TRACY_FUNC(rename);
    return UNIMPLEMENTED();
}

EXPORT(int, rewind) {
    TRACY_FUNC(rewind);
    return UNIMPLEMENTED();
}

EXPORT(int, scanf) {
    TRACY_FUNC(scanf);
    return UNIMPLEMENTED();
}

EXPORT(int, scanf_s) {
    TRACY_FUNC(scanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcFopenWithFD) {
    TRACY_FUNC(sceLibcFopenWithFD);
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcFopenWithFH) {
    TRACY_FUNC(sceLibcFopenWithFH);
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcGetFD) {
    TRACY_FUNC(sceLibcGetFD);
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcGetFH) {
    TRACY_FUNC(sceLibcGetFH);
    return UNIMPLEMENTED();
}

EXPORT(int, sceLibcSetHeapInitError) {
    TRACY_FUNC(sceLibcSetHeapInitError);
    return UNIMPLEMENTED();
}

EXPORT(int, set_constraint_handler_s) {
    TRACY_FUNC(set_constraint_handler_s);
    return UNIMPLEMENTED();
}

EXPORT(int, setbuf) {
    TRACY_FUNC(setbuf);
    return UNIMPLEMENTED();
}

// setjmp can be defined as _setjmp on some systems
#pragma push_macro("setjmp")
#undef setjmp
EXPORT(int, setjmp) {
    TRACY_FUNC(setjmp);

    return UNIMPLEMENTED();
}
#pragma pop_macro("setjmp")

EXPORT(int, setvbuf, FILE *stream, char *buffer, int mode, size_t size) {
    TRACY_FUNC(setvbuf, stream, buffer, mode, size);
    return UNIMPLEMENTED();
}

EXPORT(int, snprintf, char *s, size_t n, const char *format, module::vargs args) {
    // TODO: add args to tracy func
    TRACY_FUNC(snprintf, s, n, format);

    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    return utils::snprintf(s, n, format, *(thread->cpu), emuenv.mem, args);
}

EXPORT(int, snprintf_s) {
    TRACY_FUNC(snprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, snwprintf_s) {
    TRACY_FUNC(snwprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, sprintf) {
    TRACY_FUNC(sprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, sprintf_s) {
    TRACY_FUNC(sprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, srand) {
    TRACY_FUNC(srand);
    return UNIMPLEMENTED();
}

EXPORT(int, sscanf) {
    TRACY_FUNC(sscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, sscanf_s) {
    TRACY_FUNC(sscanf_s);
    return UNIMPLEMENTED();
}

#pragma push_macro("strcasecmp")
#undef strcasecmp
EXPORT(int, strcasecmp) {
    TRACY_FUNC(strcasecmp);
    return UNIMPLEMENTED();
}
#pragma pop_macro("strcasecmp")

EXPORT(Ptr<char>, strcat, Ptr<char> destination, Ptr<char> source) {
    TRACY_FUNC(strcat, destination, source);
    strcat(destination.get(emuenv.mem), source.get(emuenv.mem));
    return destination;
}

EXPORT(int, strcat_s) {
    TRACY_FUNC(strcat_s);
    return UNIMPLEMENTED();
}

EXPORT(int, strchr) {
    TRACY_FUNC(strchr);
    return UNIMPLEMENTED();
}

EXPORT(int, strcmp) {
    TRACY_FUNC(strcmp);
    return UNIMPLEMENTED();
}

EXPORT(int, strcoll) {
    TRACY_FUNC(strcoll);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, strcpy, Ptr<char> destination, Ptr<char> source) {
    TRACY_FUNC(strcpy, destination, source);
    strcpy(destination.get(emuenv.mem), source.get(emuenv.mem));
    return destination;
}

EXPORT(int, strcpy_s) {
    TRACY_FUNC(strcpy_s);
    return UNIMPLEMENTED();
}

EXPORT(int, strcspn) {
    TRACY_FUNC(strcspn);
    return UNIMPLEMENTED();
}

EXPORT(int, strdup) {
    TRACY_FUNC(strdup);
    return UNIMPLEMENTED();
}

EXPORT(int, strerror) {
    TRACY_FUNC(strerror);
    return UNIMPLEMENTED();
}

EXPORT(int, strerror_s) {
    TRACY_FUNC(strerror_s);
    return UNIMPLEMENTED();
}

EXPORT(int, strerrorlen_s) {
    TRACY_FUNC(strerrorlen_s);
    return UNIMPLEMENTED();
}

EXPORT(int, strftime) {
    TRACY_FUNC(strftime);
    return UNIMPLEMENTED();
}

EXPORT(int, strlen, char *str) {
    TRACY_FUNC(strlen, str);
    return static_cast<int>(strlen(str));
}

#pragma push_macro("strncasecmp")
#undef strncasecmp
EXPORT(int, strncasecmp) {
    TRACY_FUNC(strncasecmp);
    return UNIMPLEMENTED();
}
#pragma pop_macro("strncasecmp")

EXPORT(int, strncat) {
    TRACY_FUNC(strncat);
    return UNIMPLEMENTED();
}

EXPORT(int, strncat_s) {
    TRACY_FUNC(strncat_s);
    return UNIMPLEMENTED();
}

EXPORT(int, strncmp, const char *str1, const char *str2, SceSize num) {
    TRACY_FUNC(strncmp, str1, str2, num);
    return strncmp(str1, str2, num);
}

EXPORT(Ptr<char>, strncpy, Ptr<char> destination, Ptr<char> source, SceSize size) {
    TRACY_FUNC(strncpy, destination, source, size);
    strncpy(destination.get(emuenv.mem), source.get(emuenv.mem), size);
    return destination;
}

EXPORT(int, strncpy_s) {
    TRACY_FUNC(strncpy_s);
    return UNIMPLEMENTED();
}

EXPORT(int, strnlen_s) {
    TRACY_FUNC(strnlen_s);
    return UNIMPLEMENTED();
}

EXPORT(int, strpbrk) {
    TRACY_FUNC(strpbrk);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, strrchr, Ptr<char> str, char ch) {
    TRACY_FUNC(strrchr, str, ch);
    Ptr<char> res = Ptr<char>();
    char *_str = str.get(emuenv.mem);
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
    TRACY_FUNC(strspn);
    return UNIMPLEMENTED();
}

EXPORT(int, strstr) {
    TRACY_FUNC(strstr);
    return UNIMPLEMENTED();
}

EXPORT(int, strtod) {
    TRACY_FUNC(strtod);
    return UNIMPLEMENTED();
}

EXPORT(int, strtof) {
    TRACY_FUNC(strtof);
    return UNIMPLEMENTED();
}

EXPORT(int, strtoimax) {
    TRACY_FUNC(strtoimax);
    return UNIMPLEMENTED();
}

EXPORT(int, strtok) {
    TRACY_FUNC(strtok);
    return UNIMPLEMENTED();
}

EXPORT(int, strtok_r) {
    TRACY_FUNC(strtok_r);
    return UNIMPLEMENTED();
}

EXPORT(int, strtok_s) {
    TRACY_FUNC(strtok_s);
    return UNIMPLEMENTED();
}

EXPORT(int, strtol) {
    TRACY_FUNC(strtol);
    return UNIMPLEMENTED();
}

EXPORT(int, strtold) {
    TRACY_FUNC(strtold);
    return UNIMPLEMENTED();
}

EXPORT(int, strtoll) {
    TRACY_FUNC(strtoll);
    return UNIMPLEMENTED();
}

EXPORT(int, strtoul) {
    TRACY_FUNC(strtoul);
    return UNIMPLEMENTED();
}

EXPORT(int, strtoull) {
    TRACY_FUNC(strtoull);
    return UNIMPLEMENTED();
}

EXPORT(int, strtoumax) {
    TRACY_FUNC(strtoumax);
    return UNIMPLEMENTED();
}

EXPORT(int, strxfrm) {
    TRACY_FUNC(strxfrm);
    return UNIMPLEMENTED();
}

EXPORT(int, swprintf) {
    TRACY_FUNC(swprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, swprintf_s) {
    TRACY_FUNC(swprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, swscanf) {
    TRACY_FUNC(swscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, swscanf_s) {
    TRACY_FUNC(swscanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, time) {
    TRACY_FUNC(time);
    return UNIMPLEMENTED();
}

EXPORT(int, tolower) {
    TRACY_FUNC(tolower);
    return UNIMPLEMENTED();
}

EXPORT(int, toupper) {
    TRACY_FUNC(toupper);
    return UNIMPLEMENTED();
}

EXPORT(int, towctrans) {
    TRACY_FUNC(towctrans);
    return UNIMPLEMENTED();
}

EXPORT(int, towlower) {
    TRACY_FUNC(towlower);
    return UNIMPLEMENTED();
}

EXPORT(int, towupper) {
    TRACY_FUNC(towupper);
    return UNIMPLEMENTED();
}

EXPORT(int, ungetc) {
    TRACY_FUNC(ungetc);
    return UNIMPLEMENTED();
}

EXPORT(int, ungetwc) {
    TRACY_FUNC(ungetwc);
    return UNIMPLEMENTED();
}

EXPORT(int, vfprintf) {
    TRACY_FUNC(vfprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, vfprintf_s) {
    TRACY_FUNC(vfprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vfscanf) {
    TRACY_FUNC(vfscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, vfscanf_s) {
    TRACY_FUNC(vfscanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vfwprintf) {
    TRACY_FUNC(vfwprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, vfwprintf_s) {
    TRACY_FUNC(vfwprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vfwscanf) {
    TRACY_FUNC(vfwscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, vfwscanf_s) {
    TRACY_FUNC(vfwscanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vprintf) {
    TRACY_FUNC(vprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, vprintf_s) {
    TRACY_FUNC(vprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vscanf) {
    TRACY_FUNC(vscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, vscanf_s) {
    TRACY_FUNC(vscanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vsnprintf, char *s, size_t n, const char *format, Address args) {
    TRACY_FUNC(vsnprintf, s, n, format, args);

    module::vargs vargs(args);
    return CALL_EXPORT(snprintf, s, n, format, vargs);
}

EXPORT(int, vsnprintf_s) {
    TRACY_FUNC(vsnprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vsnwprintf_s) {
    TRACY_FUNC(vsnwprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vsprintf) {
    TRACY_FUNC(vsprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, vsprintf_s) {
    TRACY_FUNC(vsprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vsscanf) {
    TRACY_FUNC(vsscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, vsscanf_s) {
    TRACY_FUNC(vsscanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vswprintf) {
    TRACY_FUNC(vswprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, vswprintf_s) {
    TRACY_FUNC(vswprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vswscanf) {
    TRACY_FUNC(vswscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, vswscanf_s) {
    TRACY_FUNC(vswscanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vwprintf) {
    TRACY_FUNC(vwprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, vwprintf_s) {
    TRACY_FUNC(vwprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, vwscanf) {
    TRACY_FUNC(vwscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, vwscanf_s) {
    TRACY_FUNC(vwscanf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcrtomb) {
    TRACY_FUNC(wcrtomb);
    return UNIMPLEMENTED();
}

EXPORT(int, wcrtomb_s) {
    TRACY_FUNC(wcrtomb_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcscat) {
    TRACY_FUNC(wcscat);
    return UNIMPLEMENTED();
}

EXPORT(int, wcscat_s) {
    TRACY_FUNC(wcscat_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcschr) {
    TRACY_FUNC(wcschr);
    return UNIMPLEMENTED();
}

EXPORT(int, wcscmp) {
    TRACY_FUNC(wcscmp);
    return UNIMPLEMENTED();
}

EXPORT(int, wcscoll) {
    TRACY_FUNC(wcscoll);
    return UNIMPLEMENTED();
}

EXPORT(int, wcscpy) {
    TRACY_FUNC(wcscpy);
    return UNIMPLEMENTED();
}

EXPORT(int, wcscpy_s) {
    TRACY_FUNC(wcscpy_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcscspn) {
    TRACY_FUNC(wcscspn);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsftime) {
    TRACY_FUNC(wcsftime);
    return UNIMPLEMENTED();
}

EXPORT(int, wcslen) {
    TRACY_FUNC(wcslen);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncat) {
    TRACY_FUNC(wcsncat);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncat_s) {
    TRACY_FUNC(wcsncat_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncmp) {
    TRACY_FUNC(wcsncmp);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncpy) {
    TRACY_FUNC(wcsncpy);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsncpy_s) {
    TRACY_FUNC(wcsncpy_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsnlen_s) {
    TRACY_FUNC(wcsnlen_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcspbrk) {
    TRACY_FUNC(wcspbrk);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsrchr) {
    TRACY_FUNC(wcsrchr);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsrtombs) {
    TRACY_FUNC(wcsrtombs);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsrtombs_s) {
    TRACY_FUNC(wcsrtombs_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsspn) {
    TRACY_FUNC(wcsspn);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsstr) {
    TRACY_FUNC(wcsstr);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstod) {
    TRACY_FUNC(wcstod);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstof) {
    TRACY_FUNC(wcstof);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoimax) {
    TRACY_FUNC(wcstoimax);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstok) {
    TRACY_FUNC(wcstok);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstok_s) {
    TRACY_FUNC(wcstok_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstol) {
    TRACY_FUNC(wcstol);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstold) {
    TRACY_FUNC(wcstold);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoll) {
    TRACY_FUNC(wcstoll);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstombs) {
    TRACY_FUNC(wcstombs);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstombs_s) {
    TRACY_FUNC(wcstombs_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoul) {
    TRACY_FUNC(wcstoul);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoull) {
    TRACY_FUNC(wcstoull);
    return UNIMPLEMENTED();
}

EXPORT(int, wcstoumax) {
    TRACY_FUNC(wcstoumax);
    return UNIMPLEMENTED();
}

EXPORT(int, wcsxfrm) {
    TRACY_FUNC(wcsxfrm);
    return UNIMPLEMENTED();
}

EXPORT(int, wctob) {
    TRACY_FUNC(wctob);
    return UNIMPLEMENTED();
}

EXPORT(int, wctomb) {
    TRACY_FUNC(wctomb);
    return UNIMPLEMENTED();
}

EXPORT(int, wctomb_s) {
    TRACY_FUNC(wctomb_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wctrans) {
    TRACY_FUNC(wctrans);
    return UNIMPLEMENTED();
}

EXPORT(int, wctype) {
    TRACY_FUNC(wctype);
    return UNIMPLEMENTED();
}

EXPORT(int, wmemchr) {
    TRACY_FUNC(wmemchr);
    return UNIMPLEMENTED();
}

EXPORT(int, wmemcmp) {
    TRACY_FUNC(wmemcmp);
    return UNIMPLEMENTED();
}

EXPORT(int, wmemcpy) {
    TRACY_FUNC(wmemcpy);
    return UNIMPLEMENTED();
}

EXPORT(int, wmemcpy_s) {
    TRACY_FUNC(wmemcpy_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wmemmove) {
    TRACY_FUNC(wmemmove);
    return UNIMPLEMENTED();
}

EXPORT(int, wmemmove_s) {
    TRACY_FUNC(wmemmove_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wmemset) {
    TRACY_FUNC(wmemset);
    return UNIMPLEMENTED();
}

EXPORT(int, wprintf) {
    TRACY_FUNC(wprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, wprintf_s) {
    TRACY_FUNC(wprintf_s);
    return UNIMPLEMENTED();
}

EXPORT(int, wscanf) {
    TRACY_FUNC(wscanf);
    return UNIMPLEMENTED();
}

EXPORT(int, wscanf_s) {
    TRACY_FUNC(wscanf_s);
    return UNIMPLEMENTED();
}
