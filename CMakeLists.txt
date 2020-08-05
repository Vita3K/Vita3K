cmake_minimum_required(VERSION 3.10)
project(Vita3K)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_GDBSTUB "Build Vita3K with GDB Debugger." OFF)
option(USE_DISCORD_RICH_PRESENCE "Build Vita3K with Discord Rich Presence" OFF)
option(USE_VULKAN "Build Vita3K with Vulkan backend." OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

enable_testing()

macro(pre_configure_boost)
	find_package(Boost COMPONENTS filesystem system QUIET)

	if (Boost_FOUND)
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

if(WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	add_definitions (/D "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS" /D "_CRT_SECURE_NO_WARNINGS")
endif()

if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2 -mavx -mf16c")
endif()

# Based on source code of https://github.com/yuzu-emu/yuzu
if (WIN32)
	message(STATUS "unicorn not found, falling back to bundled")
	set(UNICORN_VER "d47829cb")

	set(UNICORN_PREFIX "${CMAKE_BINARY_DIR}/external/unicorn")
	if (NOT EXISTS "${UNICORN_PREFIX}")
		message(STATUS "Downloading unicorn...")
		
		file(DOWNLOAD
			https://github.com/Vita3K/unicorn/releases/download/${UNICORN_VER}/vita3k-unicorn.zip
			"${CMAKE_BINARY_DIR}/external/unicorn.zip" SHOW_PROGRESS)
		execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${CMAKE_BINARY_DIR}/external/unicorn.zip"
			WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external")
	endif()
	message(STATUS "Using bundled binaries at ${UNICORN_PREFIX}")

	set(UNICORN_FOUND YES)
	set(LIBUNICORN_INCLUDE_DIR "${UNICORN_PREFIX}/include" CACHE PATH "Path to Unicorn headers" FORCE)
	set(LIBUNICORN_LIBRARY "${UNICORN_PREFIX}/lib/unicorn.lib" CACHE PATH "Path to Unicorn library" FORCE)
	set(UNICORN_DLL_DIR "${UNICORN_PREFIX}/lib" CACHE PATH "Path to unicorn.dll" FORCE)
else()
	message(STATUS "unicorn not found, falling back to externals")
	if (MINGW)
		set(UNICORN_LIB_NAME "unicorn.a")
	else()
		set(UNICORN_LIB_NAME "libunicorn.a")
	endif()

	set(UNICORN_FOUND YES)
	set(UNICORN_PREFIX ${PROJECT_SOURCE_DIR}/external/unicorn)
	set(LIBUNICORN_LIBRARY "${UNICORN_PREFIX}/${UNICORN_LIB_NAME}" CACHE PATH "Path to Unicorn library" FORCE)
	set(LIBUNICORN_INCLUDE_DIR "${UNICORN_PREFIX}/include" CACHE PATH "Path to Unicorn headers" FORCE)
	set(UNICORN_DLL_DIR "${UNICORN_PREFIX}/" CACHE PATH "Path to unicorn dynamic library" FORCE)

	find_package(PythonInterp 3.5 REQUIRED)

	if (APPLE)
		add_custom_command(OUTPUT ${LIBUNICORN_LIBRARY}
			COMMAND ${CMAKE_COMMAND} -E env UNICORN_ARCHS="arm" PYTHON="${PYTHON_EXECUTABLE}" /bin/sh make.sh macos-universal-no
			WORKING_DIRECTORY ${UNICORN_PREFIX}
		)
	elseif(LINUX)
		add_custom_command(OUTPUT ${LIBUNICORN_LIBRARY}
			COMMAND ${CMAKE_COMMAND} -E env UNICORN_ARCHS="arm" PYTHON="${PYTHON_EXECUTABLE}" /bin/sh make.sh
			WORKING_DIRECTORY ${UNICORN_PREFIX}
		)
	endif()

	# ALL makes this custom target build every time
	# but it won't actually build if LIBUNICORN_LIBRARY is up to date
	add_custom_target(unicorn-build ALL
		DEPENDS ${LIBUNICORN_LIBRARY}
	)
	unset(UNICORN_LIB_NAME)
endif()

if (UNICORN_FOUND)
    add_library(unicorn INTERFACE)
    add_dependencies(unicorn unicorn-build)
    target_link_libraries(unicorn INTERFACE "${LIBUNICORN_LIBRARY}")
    target_include_directories(unicorn INTERFACE "${LIBUNICORN_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Could not find or build unicorn which is required.")
endif()

add_subdirectory(external)
add_subdirectory(vita3k)
add_subdirectory(tools/gen-modules)
