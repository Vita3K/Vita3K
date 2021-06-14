cmake_minimum_required(VERSION 3.10)
project(Vita3K)

# Detects the amount of processors of the host machine and forwards the restult to CPU_COUNT
include(ProcessorCount)
ProcessorCount(CPU_COUNT)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_DISCORD_RICH_PRESENCE "Build Vita3K with Discord Rich Presence" ON)
option(USE_VULKAN "Build Vita3K with Vulkan backend." OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

enable_testing()

############################
########## Boost ###########
############################

# Configures Boost
macro(boost_configure)
	# Sets the location of the source root and the binary installation path for Boost
	# Building Boost right at the CMake binary path should allow to build Vita3K for different target platforms
	# without having to delete the boost-build folder each time the target platform changes
	set(BOOST_SOURCEDIR "${CMAKE_SOURCE_DIR}/external/boost")
	set(BOOST_INSTALLDIR "${CMAKE_BINARY_DIR}/external/boost/")

	if (WIN32)
		execute_process(
			COMMAND ${BOOST_SOURCEDIR}/bootstrap.bat
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
		)
	elseif(UNIX)
		execute_process(
			COMMAND chmod +x ${BOOST_SOURCEDIR}/tools/build/src/engine/build.sh
			COMMAND sh ${BOOST_SOURCEDIR}/bootstrap.sh
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
		)
	endif()
endmacro(boost_configure)

# Compiles Boost
macro(boost_compile)
	if (CMAKE_SYSTEM_NAME STREQUAL Windows)
		execute_process(
			COMMAND ${BOOST_SOURCEDIR}/b2 -j${CPU_COUNT} --build-dir=${BOOST_INSTALLDIR} --stagedir=${BOOST_INSTALLDIR} toolset=msvc stage
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
		)
	elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
		execute_process(
			COMMAND ${BOOST_SOURCEDIR}/b2 -j${CPU_COUNT} --build-dir=${BOOST_INSTALLDIR} --stagedir=${BOOST_INSTALLDIR} stage
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
		)
	elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
		execute_process(
			COMMAND ${BOOST_SOURCEDIR}/b2 --ignore-site-config -j${CPU_COUNT} --build-dir=${BOOST_INSTALLDIR} --stagedir=${BOOST_INSTALLDIR} stage
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
		)
	endif()
endmacro(boost_compile)

# Adjusts CMake paths to enable Boost as a findable package for other dependencies in the project
macro(boost_set_paths)
	set (Boost_USE_STATIC_LIBS ON)
	find_package(Boost COMPONENTS filesystem system program_options QUIET)

	if (${Boost_FOUND})
		set(BOOST_INCLUDEDIR ${Boost_INCLUDE_DIRS})
		set(BOOST_LIBRARYDIR ${Boost_LIBRARIES})
	else()
		message("Setting up ext-boost environment variables")
		set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/boost")
		set(BOOST_INCLUDEDIR "${BOOST_ROOT}/boost")
		set(BOOST_LIBRARYDIR "${BOOST_INSTALLDIR}/lib")
	endif()

	message("Using Boost_VERSION: ${BOOST_ROOT}")
	message("Using Boost_INCLUDE_DIRS: ${BOOST_INCLUDEDIR}")
	message("Using Boost_LIBRARY_DIRS: ${BOOST_LIBRARYDIR}")
endmacro(boost_set_paths)



if(NOT CI)
	boost_configure()
	boost_compile()
	boost_set_paths()
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
