add_library(host_core STATIC)

# Create alias for more friendly naming and avoid conflicts
# in global CMake scope
add_library(host::core ALIAS host_core)

# Source files dependent on the target platform
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(host_core PRIVATE
        src/system_details_win.cpp
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(host_core PRIVATE
        src/system_details_linux.cpp
    )
endif()

target_include_directories(host_core PUBLIC include)
