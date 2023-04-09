cmake_minimum_required(VERSION 3.10)
project(Vita3K)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_DISCORD_RICH_PRESENCE "Build Vita3K with Discord Rich Presence" ON)
option(USE_VITA3K_UPDATE "Build Vita3K with updater." ON)

if("${CMAKE_CXX_COMPILER_LAUNCHER}" STREQUAL "")
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    endif()
endif()

if(MSVC)
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
endif()

enable_testing()

# Add Conan output path to CMake prefix path so that all dependencies are able to find
# the packages they need
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/.deps")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.deps")

message("Configuring boost...")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

find_package(Boost COMPONENTS filesystem system program_options)

message("Using Boost_VERSION: ${Boost_VERSION}")
message("Using Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message("Using Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")


# Commented until FFmpeg from Conan works with Conan 2.0
#[[
option(VITA3K_USE_SYSTEM_FFMPEG OFF)
if(NOT VITA3K_USE_SYSTEM_FFMPEG)
    # Look for FFmpeg in config mode
    message(STATUS "Looking for FFmpeg using find_package() in CONFIG mode...")
    find_package(FFmpeg CONFIG COMPONENTS avcodec avformat avutil swresample swscale REQUIRED)
else()
    find_package(PkgConfig REQUIRED)

    # Look for FFmpeg using pkg-config
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale libswresample)

    # Create interface library as a way to bundle all FFmpeg libraries into a single target
    add_library(ffmpeg INTERFACE)
    target_link_libraries(ffmpeg INTERFACE "${FFMPEG_LIBRARIES}")
    target_include_directories(ffmpeg INTERFACE "${FFMPEG_INCLUDE_DIRS}")

    # Alias the target to make its origin agnostic and match the imported name from Conan
    add_library(ffmpeg::ffmpeg ALIAS ffmpeg)
endif()
#]]

if(WIN32)
	add_definitions (/D "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS" /D "_CRT_SECURE_NO_WARNINGS" /D "NOMINMAX")
endif()

# Allow per-translation-unit parallel builds when using MSVC
if(CMAKE_GENERATOR MATCHES "Visual Studio" AND (CMAKE_C_COMPILER_ID MATCHES "MSVC|Intel|Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC|Intel|Clang"))
	string(APPEND CMAKE_C_FLAGS " /MP")
	string(APPEND CMAKE_CXX_FLAGS " /MP")
endif()

#Hide warnings in external libraries
#double msvc check is intentional (to filter out msvc-clang)
#CMAKE_CXX_COMPILER_LAUNCHER check is check if sccache is used
#sccache does not support /external flags and does not cache this compiler calls
if (MSVC AND (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") AND (MSVC_VERSION GREATER_EQUAL 1913) AND ((NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER STREQUAL "")))
	if (MSVC_VERSION LESS 1929)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /experimental:external")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /external:I${CMAKE_SOURCE_DIR}/external/ /external:W0 /external:templates-")
endif()

add_subdirectory(external)
add_subdirectory(vita3k)
add_subdirectory(tools/gen-modules)
