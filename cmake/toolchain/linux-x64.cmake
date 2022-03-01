set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x64)

# Fixes issue with setting CMAKE_SYSYEM_NAME manually and the cross-compilation check
if(CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME)
    set(CMAKE_CROSSCOMPILING FALSE)
else()
    set(CMAKE_CROSSCOMPILING TRUE)
endif()

# Compiler settings
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Disables cross-compilation
if (CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Vita3K cross-compilation for Linux isn't supported.")
endif()
