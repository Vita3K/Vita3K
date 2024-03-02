/*!
    \file cpu.h
    \brief CPU management definition
    \author Ivan Shynkarenka
    \date 27.07.2016
    \copyright MIT License
*/

#ifndef CPPCOMMON_SYSTEM_CPU_H
#define CPPCOMMON_SYSTEM_CPU_H

#include <string>

namespace CppCommon {

//! CPU management static class
/*!
    Provides CPU management functionality such as architecture, cores count,
    clock speed, Hyper-Threading feature.

    Thread-safe.
*/
class CPU {
public:
    CPU() = delete;
    CPU(const CPU &) = delete;
    CPU(CPU &&) = delete;
    ~CPU() = delete;

    CPU &operator=(const CPU &) = delete;
    CPU &operator=(CPU &&) = delete;

    //! CPU architecture string
    static std::string Architecture();
    //! CPU affinity count
    static int Affinity();
    //! CPU logical cores count
    static int LogicalCores();
    //! CPU physical cores count
    static int PhysicalCores();
    //! CPU total cores count
    static std::pair<int, int> TotalCores();
    //! CPU clock speed in Hz
    static int64_t ClockSpeed();
    //! Is CPU Hyper-Threading enabled?
    static bool HyperThreading();
};

/*! \example system_cpu.cpp CPU management example */

} // namespace CppCommon

#endif // CPPCOMMON_SYSTEM_CPU_H
