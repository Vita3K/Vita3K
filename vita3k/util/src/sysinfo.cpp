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
//
// CPU/system identification helpers. Structure adapted from RPCS3's
// util/sysinfo.cpp (GPLv2), trimmed to Vita3K's needs and extended with
// Android support.
//

#include <util/instrset_detect.h>
#include <util/sysinfo.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define SYSINFO_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define SYSINFO_ARM64 1
#endif

#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#else
#include <unistd.h> // sysconf
#endif

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#elif defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

#if defined(SYSINFO_X86)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

#if defined(SYSINFO_ARM64) && defined(__linux__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

namespace {

std::string trim(std::string s) {
    if (const auto nul = s.find('\0'); nul != std::string::npos)
        s.resize(nul);
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string::npos)
        return {};
    s = s.substr(first, s.find_last_not_of(" \t") - first + 1);

    std::string out;
    out.reserve(s.size());
    bool prev_space = false;
    for (const char c : s) {
        const bool is_space = (c == ' ');
        if (is_space && prev_space)
            continue;
        out.push_back(c);
        prev_space = is_space;
    }
    return out;
}

#if defined(SYSINFO_X86)
// Unused on macOS and Android, which read the brand from the OS instead.
[[maybe_unused]] std::array<uint32_t, 4> get_cpuid(uint32_t leaf, uint32_t subleaf) {
    int regs[4] = {};
#if defined(_MSC_VER)
    __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
#else
    __cpuid_count(leaf, subleaf, regs[0], regs[1], regs[2], regs[3]);
#endif
    return { static_cast<uint32_t>(regs[0]), static_cast<uint32_t>(regs[1]),
        static_cast<uint32_t>(regs[2]), static_cast<uint32_t>(regs[3]) };
}
#endif

#if defined(SYSINFO_ARM64)
bool arm_has_sve() {
#if defined(__linux__)
    return (getauxval(AT_HWCAP) & HWCAP_SVE) != 0;
#elif defined(__APPLE__)
    int v = 0;
    size_t len = sizeof(v);
    sysctlbyname("hw.optional.arm.FEAT_SVE", &v, &len, nullptr, 0);
    return v != 0;
#elif defined(_WIN32)
    return IsProcessorFeaturePresent(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE) != 0;
#else
    return false;
#endif
}

bool arm_has_sve2() {
#if defined(__linux__)
    return (getauxval(AT_HWCAP2) & HWCAP2_SVE2) != 0;
#elif defined(__APPLE__)
    int v = 0;
    size_t len = sizeof(v);
    sysctlbyname("hw.optional.arm.FEAT_SVE2", &v, &len, nullptr, 0);
    return v != 0;
#elif defined(_WIN32)
    return IsProcessorFeaturePresent(PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE) != 0;
#else
    return false;
#endif
}
#endif

} // namespace

namespace util {

std::string get_cpu_brand() {
#if defined(__APPLE__)
    // Works for both Intel and Apple Silicon ("Apple M2" etc.)
    char buf[256] = {};
    size_t size = sizeof(buf);
    if (sysctlbyname("machdep.cpu.brand_string", buf, &size, nullptr, 0) == 0 && buf[0])
        return trim(buf);
    return "Unknown CPU";

#elif defined(__ANDROID__)
    char value[PROP_VALUE_MAX] = {};
    // ro.soc.* exists since Android 12; older devices only expose a platform codename.
    if (__system_property_get("ro.soc.model", value) > 0) {
        std::string model = trim(value);
        if (__system_property_get("ro.soc.manufacturer", value) > 0)
            model = trim(value) + ' ' + model;
        return model;
    }
    for (const char *key : { "ro.board.platform", "ro.hardware" }) {
        if (__system_property_get(key, value) > 0)
            return trim(value);
    }
    return "Unknown CPU";

#elif defined(_WIN32) && defined(SYSINFO_ARM64)
    // No CPUID on ARM64; the registry holds the brand string.
    char buf[256] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            "ProcessorNameString", RRF_RT_REG_SZ, nullptr, buf, &size)
            == ERROR_SUCCESS
        && buf[0])
        return trim(buf);
    return "Unknown CPU";

#elif defined(SYSINFO_X86)
    if (get_cpuid(0x80000000u, 0)[0] >= 0x80000004u) {
        char brand[0x40] = {};
        for (uint32_t i = 0; i < 3; ++i) {
            const auto regs = get_cpuid(0x80000002u + i, 0);
            std::memcpy(brand + i * 16, regs.data(), 16);
        }
        return trim(brand);
    }
    return "Unknown CPU";

#elif defined(__linux__) // non-Android ARM Linux
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("model name", 0) == 0 || line.rfind("Hardware", 0) == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos)
                return trim(line.substr(colon + 1));
        }
    }
    std::ifstream model("/proc/device-tree/model");
    std::string name;
    if (std::getline(model, name) && !name.empty())
        return trim(name);
    return "Unknown CPU";
#else
    return "Unknown CPU";
#endif
}

int64_t get_cpu_clock_mhz() {
#if defined(__APPLE__)
    int64_t hz = 0;
    size_t size = sizeof(hz);
    if (sysctlbyname("hw.cpufrequency_max", &hz, &size, nullptr, 0) == 0 && hz > 0)
        return hz / 1'000'000; // Intel Macs only; Apple Silicon returns 0
    return 0;

#elif defined(_WIN32)
    DWORD mhz = 0, size = sizeof(mhz);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            "~MHz", RRF_RT_REG_DWORD, nullptr, &mhz, &size)
        == ERROR_SUCCESS)
        return static_cast<int64_t>(mhz);
    return 0;

#elif defined(__linux__)
    // cpu0 is usually a little core on big.LITTLE SoCs, so take the max across all cores.
    int64_t max_khz = 0;
    for (uint32_t i = 0;; ++i) {
        const std::string cpu_dir = "/sys/devices/system/cpu/cpu" + std::to_string(i);
        if (access(cpu_dir.c_str(), F_OK) != 0)
            break;
        std::ifstream f(cpu_dir + "/cpufreq/cpuinfo_max_freq");
        int64_t khz = 0;
        if (f >> khz)
            max_khz = std::max(max_khz, khz);
    }
    if (max_khz > 0)
        return max_khz / 1000;
    std::ifstream cpuinfo("/proc/cpuinfo"); // x86 exposes "cpu MHz"
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("cpu MHz", 0) == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos)
                return static_cast<int64_t>(std::strtod(line.c_str() + colon + 1, nullptr));
        }
    }
    return 0;
#else
    return 0;
#endif
}

std::string get_system_info() {
    std::string result = get_cpu_brand();
    result += " | " + std::to_string(std::max(std::thread::hardware_concurrency(), 1u)) + " Threads";

    if (const int64_t mhz = get_cpu_clock_mhz(); mhz > 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), " | %.2f GHz", mhz / 1000.0);
        result += buf;
    }

#if defined(SYSINFO_X86)
    const int iset = instrset::instrset_detect();
    if (iset >= instrset::instrset_AVX) {
        result += " | AVX";
        if (iset >= instrset::instrset_AVX512)
            result += "-512";
        else if (iset >= instrset::instrset_AVX2)
            result += '+';
        if (instrset::hasXOP())
            result += 'x';
    }
    if (instrset::hasFMA3() || instrset::hasFMA4()) {
        result += " | FMA";
        if (instrset::hasFMA3() && instrset::hasFMA4())
            result += "3+4";
        else
            result += instrset::hasFMA3() ? '3' : '4';
    }
#elif defined(SYSINFO_ARM64)
    if (arm_has_sve())
        result += arm_has_sve2() ? " | SVE2" : " | SVE";
    else
        result += " | NEON";
#endif

    return result;
}

uint64_t get_total_memory() {
#if defined(_WIN32)
    MEMORYSTATUSEX mem_info{};
    mem_info.dwLength = sizeof(mem_info);
    GlobalMemoryStatusEx(&mem_info);
    return mem_info.ullTotalPhys;
#elif defined(__APPLE__)
    int64_t mem = 0;
    size_t size = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &size, nullptr, 0) == 0 && mem > 0)
        return static_cast<uint64_t>(mem);
    return 0;
#else // Unix-like
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0)
        return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
    return 0;
#endif
}

uint64_t get_process_memory_usage() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    // The K32 variant lives in kernel32, so psapi.lib is not needed.
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize;
    return 0;

#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
            reinterpret_cast<task_info_t>(&info), &count)
        == KERN_SUCCESS)
        return info.resident_size;
    return 0;

#elif defined(__linux__)
    // /proc/self/statm: "size resident shared text lib data dt" (in pages)
    std::ifstream statm("/proc/self/statm");
    uint64_t total_pages = 0, resident_pages = 0;
    if (statm >> total_pages >> resident_pages) {
        const long page_size = sysconf(_SC_PAGESIZE);
        if (page_size > 0)
            return resident_pages * static_cast<uint64_t>(page_size);
    }
    return 0;

#else
    return 0;
#endif
}

} // namespace util
