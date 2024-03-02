/*!
    \file environment.h
    \brief Environment management definition
    \author Ivan Shynkarenka
    \date 27.07.2016
    \copyright MIT License
*/

#ifndef CPPCOMMON_SYSTEM_ENVIRONMENT_H
#define CPPCOMMON_SYSTEM_ENVIRONMENT_H

#include <string>

namespace CppCommon {

//! Environment management static class
/*!
    Provides environment management functionality to get OS bit version, process bit version,
    debug/release mode.

    Thread-safe.
*/

class Environment {
public:
    //! Get OS version string
    static std::string OSVersion();
};

/*! \example system_environment.cpp Environment management example */

} // namespace CppCommon

#endif // CPPCOMMON_SYSTEM_ENVIRONMENT_H
