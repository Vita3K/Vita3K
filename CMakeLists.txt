cmake_minimum_required(VERSION 3.10)
project(Vita3K)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_GDBSTUB "Build Vita3K with GDB Debugger." OFF)
option(USE_DISCORD_RICH_PRESENCE "Build Vita3K with Discord Rich Presence" ON)
option(USE_VULKAN "Build Vita3K with Vulkan backend." OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

enable_testing()

macro(pre_configure_boost)
	set (Boost_USE_STATIC_LIBS ON)
	find_package(Boost COMPONENTS filesystem system program_options QUIET)

	if (${Boost_FOUND})
		set(BOOST_INCLUDEDIR ${Boost_INCLUDE_DIRS})
		set(BOOST_LIBRARYDIR ${Boost_LIBRARIES})
	else()
		message("Setting up ext-boost environment variables")
		set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/boost")
		set(BOOST_INCLUDEDIR "${BOOST_ROOT}/boost")
		set(BOOST_LIBRARYDIR "${CMAKE_CURRENT_SOURCE_DIR}/external/boost-build/lib")
	endif()

	message("Using Boost_VERSION: ${BOOST_ROOT}")
	message("Using Boost_INCLUDE_DIRS: ${BOOST_INCLUDEDIR}")
	message("Using Boost_LIBRARY_DIRS: ${BOOST_LIBRARYDIR}")
endmacro(pre_configure_boost)

if(NOT CI)
	pre_configure_boost()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GetStandard.cmake")
get_standard_for_build()
if(${CPP17_SUPPORTED})
	set(STANDARD_NUM 17)
else()
	set(STANDARD_NUM 11)
endif()

if(WIN32)
	add_definitions (/D "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS" /D "_CRT_SECURE_NO_WARNINGS" /D "NOMINMAX")
endif()

# Allow per-translation-unit parallel builds when using MSVC
if(CMAKE_GENERATOR MATCHES "Visual Studio" AND (CMAKE_C_COMPILER_ID MATCHES "MSVC|Intel" OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC|Intel"))
	string(APPEND CMAKE_C_FLAGS " /MP")
	string(APPEND CMAKE_CXX_FLAGS " /MP")
endif()

if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2 -mavx -mf16c")
endif()

add_subdirectory(external)
add_subdirectory(vita3k)
add_subdirectory(tools/gen-modules)
