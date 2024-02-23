/*!
    \file cpu.cpp
    \brief CPU management implementation
    \author Ivan Shynkarenka
    \date 27.07.2016
    \copyright MIT License
*/

#include "include/cpu.h"
#include "include/resource.h"

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(unix) || defined(__unix) || defined(__unix__)
#include <fstream>
#include <regex>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace CppCommon {

//! @cond INTERNALS
namespace Internals {

#if defined(_WIN32) || defined(_WIN64)

// Helper function to count set bits in the processor mask
DWORD CountSetBits(ULONG_PTR pBitMask) {
    DWORD dwLeftShift = sizeof(ULONG_PTR) * 8 - 1;
    DWORD dwBitSetCount = 0;
    ULONG_PTR pBitTest = (ULONG_PTR)1 << dwLeftShift;

    for (DWORD i = 0; i <= dwLeftShift; ++i) {
        dwBitSetCount += ((pBitMask & pBitTest) ? 1 : 0);
        pBitTest /= 2;
    }

    return dwBitSetCount;
}

#endif

} // namespace Internals
//! @endcond

std::string CPU::Architecture() {
#if defined(__APPLE__)
    char result[1024];
    size_t size = sizeof(result);
    if (sysctlbyname("machdep.cpu.brand_string", result, &size, nullptr, 0) == 0)
        return result;

    return "<unknown>";
#elif defined(unix) || defined(__unix) || defined(__unix__)
    static std::regex pattern("model name(.*): (.*)");

    std::string line;
    std::ifstream stream("/proc/cpuinfo");
    while (getline(stream, line)) {
        std::smatch matches;
        if (std::regex_match(line, matches, pattern))
            return matches[2];
    }

    return "<unknown>";
#elif defined(_WIN32) || defined(_WIN64)
    HKEY hKeyProcessor;
    LONG lError = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKeyProcessor);
    if (lError != ERROR_SUCCESS)
        return "<unknown>";

    // Smart resource cleaner pattern
    auto key = resource(hKeyProcessor, [](HKEY hKey) { RegCloseKey(hKey); });

    CHAR pBuffer[_MAX_PATH] = { 0 };
    DWORD dwBufferSize = sizeof(pBuffer);
    lError = RegQueryValueExA(key.get(), "ProcessorNameString", nullptr, nullptr, (LPBYTE)pBuffer, &dwBufferSize);
    if (lError != ERROR_SUCCESS)
        return "<unknown>";

    // Remove trailing 0x20 space from pBuffer
    while (!std::isgraph(pBuffer[dwBufferSize - 1]) && dwBufferSize > 0) {
        pBuffer[--dwBufferSize] = '\0';
    }

    return std::string(pBuffer);
#else
#error Unsupported platform
#endif
}

int CPU::Affinity() {
#if defined(__APPLE__)
    int logical = 0;
    size_t logical_size = sizeof(logical);
    if (sysctlbyname("hw.logicalcpu", &logical, &logical_size, nullptr, 0) != 0)
        logical = -1;

    return logical;
#elif defined(unix) || defined(__unix) || defined(__unix__)
    long processors = sysconf(_SC_NPROCESSORS_ONLN);
    return processors;
#elif defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
#else
#error Unsupported platform
#endif
}

int CPU::LogicalCores() {
    return TotalCores().first;
}

int CPU::PhysicalCores() {
    return TotalCores().second;
}

std::pair<int, int> CPU::TotalCores() {
#if defined(__APPLE__)
    int logical = 0;
    size_t logical_size = sizeof(logical);
    if (sysctlbyname("hw.logicalcpu", &logical, &logical_size, nullptr, 0) != 0)
        logical = -1;

    int physical = 0;
    size_t physical_size = sizeof(physical);
    if (sysctlbyname("hw.physicalcpu", &physical, &physical_size, nullptr, 0) != 0)
        physical = -1;

    return std::make_pair(logical, physical);
#elif defined(unix) || defined(__unix) || defined(__unix__)
    long processors = sysconf(_SC_NPROCESSORS_ONLN);
    return std::make_pair(processors, processors);
#elif defined(_WIN32) || defined(_WIN64)
    BOOL allocated = FALSE;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = nullptr;
    DWORD dwLength = 0;

    while (!allocated) {
        BOOL bResult = GetLogicalProcessorInformation(pBuffer, &dwLength);
        if (bResult == FALSE) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                if (pBuffer != nullptr)
                    std::free(pBuffer);
                pBuffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)std::malloc(dwLength);
                if (pBuffer == nullptr)
                    return std::make_pair(-1, -1);
            } else
                return std::make_pair(-1, -1);
        } else
            allocated = TRUE;
    }

    std::pair<int, int> result(0, 0);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pCurrent = pBuffer;
    DWORD dwOffset = 0;

    while (dwOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= dwLength) {
        switch (pCurrent->Relationship) {
        case RelationProcessorCore:
            result.first += Internals::CountSetBits(pCurrent->ProcessorMask);
            result.second += 1;
            break;
        case RelationNumaNode:
        case RelationCache:
        case RelationProcessorPackage:
            break;
        default:
            return std::make_pair(-1, -1);
        }
        dwOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        pCurrent++;
    }

    std::free(pBuffer);

    return result;
#else
#error Unsupported platform
#endif
}

int64_t CPU::ClockSpeed() {
#if defined(__APPLE__)
    uint64_t frequency = 0;
    size_t size = sizeof(frequency);
    if (sysctlbyname("hw.cpufrequency", &frequency, &size, nullptr, 0) == 0)
        return frequency;

    return -1;
#elif defined(unix) || defined(__unix) || defined(__unix__)
    static std::regex pattern("cpu MHz(.*): (.*)");

    std::string line;
    std::ifstream stream("/proc/cpuinfo");
    while (getline(stream, line)) {
        std::smatch matches;
        if (std::regex_match(line, matches, pattern))
            return (int64_t)(atof(matches[2].str().c_str()));
    }

    return -1;
#elif defined(_WIN32) || defined(_WIN64)
    HKEY hKeyProcessor;
    long lError = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKeyProcessor);
    if (lError != ERROR_SUCCESS)
        return -1;

    // Smart resource cleaner pattern
    auto key = resource(hKeyProcessor, [](HKEY hKey) { RegCloseKey(hKey); });

    DWORD dwMHz = 0;
    DWORD dwBufferSize = sizeof(DWORD);
    lError = RegQueryValueExA(key.get(), "~MHz", nullptr, nullptr, (LPBYTE)&dwMHz, &dwBufferSize);
    if (lError != ERROR_SUCCESS)
        return -1;

    return dwMHz;
#else
#error Unsupported platform
#endif
}

bool CPU::HyperThreading() {
    std::pair<int, int> cores = TotalCores();
    return (cores.first != cores.second);
}

} // namespace CppCommon
