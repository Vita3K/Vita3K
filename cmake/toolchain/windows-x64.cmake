set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x64)

# Fixes issue with setting CMAKE_SYSYEM_NAME manually and the cross-compilation check
if(CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME)
    set(CMAKE_CROSSCOMPILING FALSE)
else()
    set(CMAKE_CROSSCOMPILING TRUE)
endif()

# Compiler settings
set(CMAKE_C_COMPILER cl)
set(CMAKE_CXX_COMPILER cl)

if (CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Vita3K cross-compilation for Windows isn't supported.")
endif()
