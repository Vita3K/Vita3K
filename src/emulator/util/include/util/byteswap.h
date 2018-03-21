#pragma once

#ifndef _MSC_VER
#define swap_short __builtin_bswap16
#define swap_int __builtin_bswap32
#define swap_int64 __builtin_bswap64
#else
#define swap_short _byteswap_ushort
#define swap_int _byteswap_ulong
#define swap_int64 _byteswap_uint64
#endif