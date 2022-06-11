cmake_minimum_required(VERSION 3.10)
project(Vita3K)

# Detects the amount of processors of the host machine and forwards the result to CPU_COUNT
include(ProcessorCount)
ProcessorCount(CPU_COUNT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_DISCORD_RICH_PRESENCE "Build Vita3K with Discord Rich Presence" ON)
option(USE_VULKAN "Build Vita3K with Vulkan backend." OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

if(MSVC)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
		string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
		string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
	elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
		string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
	endif()
endif()

enable_testing()

############################
########## Boost ###########
############################

# Macro to configure Boost
macro(boost_configure)
	if (WIN32)
		execute_process(
			COMMAND ${BOOST_SOURCEDIR}/bootstrap.bat --with-toolset=${BOOST_TOOLSET}
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

# Macro to compile Boost
macro(boost_compile)
	if (CMAKE_SYSTEM_NAME STREQUAL Windows)
		execute_process(
			COMMAND ${BOOST_SOURCEDIR}/b2 -j${CPU_COUNT} --build-dir=${BOOST_INSTALLDIR} --stagedir=${BOOST_INSTALLDIR} address-model=64 --architecture=x64 toolset=${BOOST_TOOLSET} stage
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

# Macro to adjust CMake paths to enable Boost as a static library and findable package for other dependencies in the project
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

# Builds Boost using source code located at external/boost if the host isn't a CI environment
if(NOT CI)
	# Sets the location of the source root and the binary installation path for Boost
	# Building Boost right at the CMake binary path should allow to build Vita3K for different target platforms
	# without having to delete the boost-build folder each time the target platform changes
	set(BOOST_SOURCEDIR "${CMAKE_SOURCE_DIR}/external/boost")
	set(BOOST_INSTALLDIR "${CMAKE_BINARY_DIR}/external/boost/")

	# Prevents the function from using CMake cache to retrieve the path and
	# avoid errors in case b2 was deleted between cache generation events
	unset(b2 CACHE)
	
	#set toolset for windows
	if (WIN32)
		if (CMAKE_GENERATOR_TOOLSET STREQUAL clangcl)
			set(BOOST_TOOLSET clang-win)
		else()	
			set(BOOST_TOOLSET msvc)
		endif()
	endif()

	# Skips Boost configuration and b2 compilation if it already exists by trying to find b2
	# b2 compilation takes a while and time for consecutive cache re-generations can be reduced this way
	if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
		find_file(
			# Variable which holds the path to b2 if it's found
			# If not, its value will be "b2-NOTFOUND"
			b2

			# Name of the file to look for depending on the host OS
			b2.exe

			PATHS "${BOOST_SOURCEDIR}"
			
			# Prevents the function from using CMake cache to retrieve the path and
			# avoid errors in case b2 was deleted between cache generation events
			NO_CMAKE_PATH
		)
	else()
		find_file(
			# Variable which holds the path to b2 if it's found
			# If not, its value will be "b2-NOTFOUND"
			b2

			# Name of the file to look for depending on the host OS
			b2

			PATHS "${BOOST_SOURCEDIR}"
		)
	endif()

	if (${b2} STREQUAL "b2-NOTFOUND")
		boost_configure()
	endif()

	boost_compile()
	boost_set_paths()
endif()

if(WIN32)
	add_definitions (/D "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS" /D "_CRT_SECURE_NO_WARNINGS" /D "NOMINMAX")
endif()

# Allow per-translation-unit parallel builds when using MSVC
if(CMAKE_GENERATOR MATCHES "Visual Studio" AND (CMAKE_C_COMPILER_ID MATCHES "MSVC|Intel|Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC|Intel|Clang"))
	string(APPEND CMAKE_C_FLAGS " /MP")
	string(APPEND CMAKE_CXX_FLAGS " /MP")
endif()

add_subdirectory(external)
add_subdirectory(vita3k)
add_subdirectory(tools/gen-modules)
