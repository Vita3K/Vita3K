cmake_minimum_required(VERSION 3.10)
project(Vita3K)

option(USE_GDBSTUB "Build Vita3K with GDB Debugger." OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

enable_testing()

macro(pre_configure_boost)
	find_package(Boost COMPONENTS filesystem system program_options QUIET)

	if (Boost_FOUND)
		set(BOOST_INCLUDEDIR ${Boost_INCLUDE_DIRS})
		set(BOOST_LIBRARYDIR ${Boost_LIBRARIES})
	else()
		message("Setting up ext-boost environment variables")
		set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/external/boost")
		set(BOOST_INCLUDEDIR "${BOOST_ROOT}/boost")
		set(BOOST_LIBRARYDIR "${CMAKE_CURRENT_SOURCE_DIR}/src/external/boost-build/lib")
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
	add_definitions (/D "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS")
endif()

add_subdirectory(src/external)
add_subdirectory(src/emulator)
add_subdirectory(src/gen-modules)
