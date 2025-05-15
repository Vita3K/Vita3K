cmake_minimum_required(VERSION 3.17.2)
project(Vita3K)

# Detects the amount of processors of the host machine and forwards the result to CPU_COUNT
include(ProcessorCount)
ProcessorCount(CPU_COUNT)

# Define the Architecture variable, right now it should only contain "x86_64" or "arm64"
include("external/dynarmic/CMakeModules/DetectArchitecture.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

# workaround for SDL due to cmake bug 
# https://github.com/libsdl-org/SDL/issues/6454
# https://gitlab.kitware.com/cmake/cmake/-/issues/24104
if(APPLE)
    enable_language(OBJC)
endif()

option(USE_DISCORD_RICH_PRESENCE "Build Vita3K with Discord Rich Presence" ON)
option(USE_VITA3K_UPDATE "Build Vita3K with updater." ON)
option(BUILD_APPIMAGE "Build an AppImage." OFF)

option(FORCE_BUILD_OPENSSL_MAC OFF)

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

cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

set(lto_usage_mode ALWAYS RELEASE_ONLY NEVER)
set(USE_LTO RELEASE_ONLY CACHE STRING "Use interprocedural optimization/link time optimization")
set_property(CACHE USE_LTO PROPERTY STRINGS ${lto_usage_mode})

if (NOT (USE_LTO STREQUAL "NEVER"))
	include(CheckIPOSupported)
	check_ipo_supported(RESULT ipo_supported OUTPUT ipo_supported_error)

	if( ipo_supported )
		if (USE_LTO STREQUAL "ALWAYS") 
			set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
		elseif (USE_LTO STREQUAL "RELEASE_ONLY")
			set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
		endif()
	else()
		message(STATUS "IPO / LTO not supported: <${ipo_supported_error}>")
		set(USE_LTO NEVER CACHE STRING "Use interprocedural optimization/link time optimization")
	endif()
endif()


############################
########## Boost ###########
############################

# Macro to build Boost buildsystem executable
macro(b2_build)
	if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
		execute_process(
			COMMAND ${BOOST_SOURCEDIR}/bootstrap.bat --with-toolset=${BOOST_TOOLSET}
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
			OUTPUT_QUIET
		)
	elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
		execute_process(
			COMMAND chmod +x ${BOOST_SOURCEDIR}/tools/build/src/engine/build.sh
			COMMAND sh ${BOOST_SOURCEDIR}/bootstrap.sh
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
			OUTPUT_QUIET
		)
	elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
		execute_process(
			COMMAND chmod +x ${BOOST_SOURCEDIR}/tools/build/src/engine/build.sh
			COMMAND sh ${BOOST_SOURCEDIR}/bootstrap.sh --with-toolset=${BOOST_TOOLSET}
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
			OUTPUT_QUIET
		)
	endif()
endmacro(b2_build)

# Macro to compile Boost
macro(boost_compile)
	list(TRANSFORM BOOST_MODULES_TO_FIND PREPEND --with- OUTPUT_VARIABLE b2_cmd_line_common)
	set(b2_cmd_line_common ${b2} ${b2_cmd_line_common} -j${CPU_COUNT} --build-dir=${BOOST_INSTALLDIR} --stagedir=${BOOST_INSTALLDIR} cxxflags=${BOOST_CXX_FLAGS})
	set(Boost_USE_DEBUG_RUNTIME "")
	if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
		execute_process(
			COMMAND ${b2_cmd_line_common} address-model=64 --architecture=x64 toolset=${BOOST_TOOLSET} variant=debug,release stage
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
			OUTPUT_QUIET
		)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		execute_process(
			COMMAND ${b2_cmd_line_common} architecture=${BOOST_ARCHITECTURE} cxxflags="-mmacosx-version-min=11.0" stage
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
			OUTPUT_QUIET
		)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		execute_process(
			COMMAND ${b2_cmd_line_common} --ignore-site-config toolset=${BOOST_TOOLSET} stage
			WORKING_DIRECTORY ${BOOST_SOURCEDIR}
			OUTPUT_QUIET
		)
	endif()
endmacro(boost_compile)

option(VITA3K_FORCE_CUSTOM_BOOST "Force Vita3K build process to use the Boost version included with the repository" OFF)
option(VITA3K_FORCE_SYSTEM_BOOST "Force Vita3K build process to use the Boost version in the system and CMake's default paths and ignore the Boost version included with Vita3K" OFF)

# Boost modules to be found by CMake
set(BOOST_MODULES_TO_FIND filesystem system)

# If build process isn't set to forcefully use system Boost
macro(get_boost)
	if(NOT VITA3K_FORCE_SYSTEM_BOOST)
		# find_package(Boost ...) setting
		set(Boost_USE_STATIC_LIBS ON)

		set(Boost_NO_WARN_NEW_VERSIONS 1)

		# First, try to find Boost without any hints (system Boost)
		if(NOT VITA3K_FORCE_CUSTOM_BOOST)
			find_package(Boost 1.81 COMPONENTS ${BOOST_MODULES_TO_FIND} QUIET)
		endif()

		# If system Boost hasn't been found, then enable hints to find custom Boost
		if (NOT Boost_FOUND)
			message(STATUS "A Boost distribution that meets the requirements needed for the project hasn't been found in your system. Falling back to custom Boost distribution...")

			# Set path hints for Boost search inside the repository folder and assist in compilation if needed
			set(BOOST_SOURCEDIR "${CMAKE_SOURCE_DIR}/external/boost")       # For internal use
			set(BOOST_INSTALLDIR "${CMAKE_BINARY_DIR}/external/boost")      # For internal use
			set(BOOST_ROOT "${BOOST_SOURCEDIR}")                            # find_package(Boost ...) hint
			set(BOOST_INCLUDEDIR "${BOOST_SOURCEDIR}")                      # find_package(Boost ...) hint
			set(BOOST_LIBRARYDIR "${BOOST_INSTALLDIR}/lib")                 # find_package(Boost ...) hint

			# Find Boost again to check for a existing compilation of the custom distribution
			find_package(Boost 1.81 COMPONENTS ${BOOST_MODULES_TO_FIND} PATHS ${BOOST_INSTALLDIR} NO_DEFAULT_PATH)

			# If no build of the custom distribution is found, compile it
			if(NOT Boost_FOUND)
				message(STATUS "No existing custom distribution of Boost has been found in ${BOOST_INSTALLDIR}. Proceeding to compile Boost...")

				# Set compilation toolset for b2 and Boost builds
				if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
					if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
						if(CMAKE_GENERATOR_TOOLSET STREQUAL "ClangCL")
							# Proper toolset should be "clang-win", but b2 has a bug that makes it spam Visual Studio instances if set
							# Since Clang for Windows is ABI-compatible with MSVC, using MSVC is fine
							set(BOOST_TOOLSET "msvc")
						else()
							message(WARNING "Boost compilation on Windows requires the use of a clang-cl compiler running inside a Visual Studio Developer CLI environment with a Windows C++ SDK available.")
							set(BOOST_TOOLSET "msvc")
						endif()
					else()
						set(BOOST_TOOLSET "msvc")
					endif()
				elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
					if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
						set(BOOST_TOOLSET "clang")
					elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
						set(BOOST_TOOLSET "gcc")
					endif()
				elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
					set(BOOST_TOOLSET "clang-darwin")
					if (ARCHITECTURE STREQUAL "x86_64")
						set(BOOST_ARCHITECTURE "x86")
					elseif (ARCHITECTURE STREQUAL "arm64")
						set(BOOST_ARCHITECTURE "arm")
					else()
						set(BOOST_ARCHITECTURE "arm+x86")
					endif()
				endif()

				# Determine b2 executable name depending on host system
				if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
					set(b2_EXECUTABLE_NAME "b2.exe")
				else()
					set(b2_EXECUTABLE_NAME "b2")
				endif()

				# Find b2
				find_program(b2 NAME ${b2_EXECUTABLE_NAME} PATHS ${BOOST_SOURCEDIR} NO_CACHE NO_DEFAULT_PATH)

				# Compile b2 if it isn't found
				if(b2 STREQUAL "b2-NOTFOUND")
					message(STATUS "The b2 buildsystem executable hasn't been found in your system. Compiling b2...")
					b2_build()

					# Find b2 again after compilation
					find_program(b2 NAME ${b2_EXECUTABLE_NAME} PATHS ${BOOST_SOURCEDIR} NO_CACHE NO_DEFAULT_PATH REQUIRED)
				endif()

				# Compile Boost
				message(STATUS "Compiling Boost...")
				boost_compile()

				# Find Boost again
				find_package(Boost 1.81 COMPONENTS ${BOOST_MODULES_TO_FIND} PATHS ${BOOST_INSTALLDIR} NO_DEFAULT_PATH REQUIRED)
			else()
				message(STATUS "Custom Boost distribution found in ${BOOST_INSTALLDIR}")
			endif()
		endif()
	else()
		# Try to find Boost on the system and CMake's default paths
		set(Boost_USE_STATIC_LIBS ON)
		find_package(Boost 1.81 COMPONENTS ${BOOST_MODULES_TO_FIND} REQUIRED)
	endif()
endmacro(get_boost)

get_boost()

if(WIN32)
	add_compile_definitions("_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS" "_CRT_SECURE_NO_WARNINGS" "NOMINMAX")
endif()

# Allow per-translation-unit parallel builds when using MSVC
if(CMAKE_GENERATOR MATCHES "Visual Studio" AND (CMAKE_C_COMPILER_ID MATCHES "MSVC|Intel|Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC|Intel|Clang"))
	string(APPEND CMAKE_C_FLAGS " /MP")
	string(APPEND CMAKE_CXX_FLAGS " /MP")
endif()

if (${BUILD_APPIMAGE})
	if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(LINUXDEPLOY_COMMAND "${CMAKE_SOURCE_DIR}/appimage/linuxdeploy.AppImage" CACHE INTERNAL "")
		if (NOT EXISTS "${LINUXDEPLOY_COMMAND}")
			message(FATAL_ERROR "Could not find linuxdeploy at ${LINUXDEPLOY_COMMAND}!")
		endif()
	else()
		message(FATAL_ERROR "Cannot build an AppImage for a non-Linux host.")
	endif()
endif()

add_subdirectory(external)
add_subdirectory(vita3k)
add_subdirectory(tools/gen-modules)
