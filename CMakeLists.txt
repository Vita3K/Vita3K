cmake_minimum_required(VERSION 3.2)
project(Vita3K)

option(USE_GDBSTUB "Build Vita3K with GDB Debugger." OFF)
option(USE_DISCORD_RICH_PRESENCE "Build Vita3K with Discord Rich Presence" ON)
option(USE_VULKAN "Build Vita3K with Vulkan backend." OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

enable_testing()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
endif()

include(ExternalProject)
function(check_submodules_present)
    file(READ "${CMAKE_SOURCE_DIR}/.gitmodules" gitmodules)
    string(REGEX MATCHALL "path *= *[^ \t\r\n]*" gitmodules ${gitmodules})
    foreach(module ${gitmodules})
        string(REGEX REPLACE "path *= *" "" module ${module})
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${module}/.git")
            message(FATAL_ERROR "Git submodule ${module} not found. "
                    "Please run: git submodule update --init --recursive")
        endif()
    endforeach()
endfunction()
check_submodules_present()

# Note: Mac on Travis is not yet updated to C++17.
include("${PROJECT_SOURCE_DIR}/cmake/GetStandard.cmake")
get_standard_for_build()
if(NOT COMPILE_MODERN_CPP)
    set(BOOST_COMPONENTS filesystem system)
    include("${PROJECT_SOURCE_DIR}/cmake/ConfigureBoost.cmake")
    if(NOT CI)
        setup_boost()
    endif()
    set(STANDARD_NUM 11)
else()
    if(CPP17_SUPPORTED)
        set(STANDARD_NUM 17)
    elseif(CPP14_SUPPORTED)
        set(STANDARD_NUM 14)
    endif()
    if(WIN32)
        set(CMAKE_CXX_FLAGS ${STANDARD_FLAG})
    endif()
endif()

set(CMAKE_CXX_STANDARD ${STANDARD_NUM})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT WIN32)
    list(APPEND CMAKE_CXX_FLAGS ${STANDARD_FLAG})
endif()

if(WIN32)
    add_definitions("/MP" # Multi-processor compile
                    "/Zc:__cplusplus" # Force defining and building as C++
                    "/EHsc" # C functions wrapped in extern "C" don't throw an exception
                    /D "WIN32" # Force definition of WIN32 macro (needed for submodules)
                    /D "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS"
                    /D "_CRT_SECURE_NO_WARNINGS")
endif()

if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2 -mavx -mf16c")
endif()

add_subdirectory(external)
add_subdirectory(vita3k)
add_subdirectory(tools/gen-modules)
