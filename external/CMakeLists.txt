add_compile_options(-w)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

# Make option() in all external projects do nothing when a normal variable of the
# same name exists. The normal variable is not removed. The cache entry is not
# created or updated and is ignored if it exists.
set(CMAKE_POLICY_DEFAULT_CMP0077 "NEW")

function(check_submodules_present)
	file(READ "${CMAKE_SOURCE_DIR}/.gitmodules" gitmodules)
	string(REGEX MATCHALL "path *= *[^ \t\r\n]*" gitmodules ${gitmodules})

	foreach(module ${gitmodules})
		# Get module name
		string(REGEX REPLACE "path *= *" "" module ${module})

		# Stat the folder and get amount of entries
		file(GLOB RESULT "${CMAKE_SOURCE_DIR}/${module}/*")
		list(LENGTH RESULT RES_LEN)

		# If the module has no files, bring fatal error
		if(RES_LEN EQUAL 0)
			# directory is empty
			message(FATAL_ERROR "Submodule ${module} is empty. "
				"Please run: git submodule update --init --recursive")
		endif()
	endforeach()
endfunction()

check_submodules_present()

# Set folder for external projects (for project tree in IDE)
set(CMAKE_FOLDER externals)

option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(CAPSTONE_ARCHITECTURE_DEFAULT "Whether architectures are enabled by default" OFF)
option(CAPSTONE_ARM_SUPPORT "ARM support" ON)
add_subdirectory(capstone EXCLUDE_FROM_ALL)

add_library(dirent INTERFACE)

if(MSVC)
	target_include_directories(dirent INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/dirent/include")
endif()

add_library(printf INTERFACE)
target_include_directories(printf INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/printf")

set(FMT_INSTALL ON)
add_subdirectory(fmt)
set(fmt_DIR fmt)

set(SPDLOG_WCHAR_FILENAMES ON CACHE BOOL "")
set(SPDLOG_NO_THREAD_ID ON CACHE BOOL "")
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "")
add_subdirectory(spdlog EXCLUDE_FROM_ALL)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb")

add_library(ddspp INTERFACE)
target_include_directories(ddspp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/ddspp")

add_library(cppcommon STATIC CppCommon/source/cpu.cpp CppCommon/source/environment.cpp)
target_include_directories(cppcommon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/CppCommon")

add_library(glad STATIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/src/gl.c")
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/include")

add_library(better-enums INTERFACE)
target_include_directories(better-enums INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/better-enums")
target_compile_definitions(better-enums INTERFACE BETTER_ENUMS_STRICT_CONVERSION=1)

add_library(googletest STATIC googletest/googletest/src/gtest_main.cc googletest/googletest/src/gtest-all.cc)
target_include_directories(googletest PUBLIC googletest/googletest/include)
target_include_directories(googletest PRIVATE googletest/googletest)
target_compile_definitions(googletest PUBLIC GTEST_HAS_PTHREAD=0)

add_library(concurrentqueue INTERFACE)
target_include_directories(concurrentqueue INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/concurrentqueue")

add_subdirectory(libfat16)

# The imgui target is including both imgui and imgui_club.
add_library(imgui STATIC imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/misc/cpp/imgui_stdlib.cpp)
target_compile_definitions(imgui PRIVATE IMGUI_DISABLE_DEMO_WINDOWS)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui_club/imgui_memory_editor/")

add_library(miniz STATIC miniz/miniz.c miniz/miniz.h)
target_include_directories(miniz PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/miniz")

add_subdirectory(sdl)

# Cubeb setup, from https://github.com/RPCS3/rpcs3/blob/master/3rdparty/cubeb/CMakeLists.txt
set(BUILD_SHARED_LIBS FALSE CACHE BOOL "Don't build shared libs")
set(BUILD_TESTS FALSE CACHE BOOL "Don't build tests")
set(BUILD_RUST_LIBS FALSE CACHE BOOL "Don't build rust libs")
set(BUILD_TOOLS FALSE CACHE BOOL "Don't build tools")
set(BUNDLE_SPEEX TRUE CACHE BOOL "Bundle the speex library")
set(LAZY_LOAD_LIBS TRUE CACHE BOOL "Lazily load shared libraries")
set(USE_SANITIZERS FALSE CACHE BOOL "Dont't use sanitizers")

add_subdirectory(cubeb EXCLUDE_FROM_ALL)

if (ARCHITECTURE STREQUAL "arm64")
	target_compile_definitions(speex PUBLIC
		#_USE_NEON
	)
elseif (ARCHITECTURE STREQUAL "x86_64")
	target_compile_definitions(speex PUBLIC
		_USE_SSE
		_USE_SSE2
	)
endif ()

if(WIN32)
	add_library(winsock INTERFACE)
	find_library(WSOCK32 wsock32)
	find_library(WS2_32 ws2_32)
	find_library(IPHLPAPI iphlpapi)
	target_link_libraries(winsock INTERFACE WSOCK32 WS2_32 IPHLPAPI)
endif()

set(DYNARMIC_TESTS OFF CACHE BOOL "")
set(DYNARMIC_NO_BUNDLED_FMT ON CACHE BOOL "")
set(DYNARMIC_FRONTENDS "A32" CACHE STRING "")
add_subdirectory(dynarmic)

if(MSVC)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		get_target_property(MCL_COMPILE_OPTIONS mcl COMPILE_OPTIONS)
		list(REMOVE_ITEM MCL_COMPILE_OPTIONS /std:c++latest)
		set_property(TARGET mcl PROPERTY COMPILE_OPTIONS ${MCL_COMPILE_OPTIONS})
	endif()
endif()

add_library(vita-toolchain INTERFACE)
target_include_directories(vita-toolchain INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vita-toolchain/src")

option(YAML_CPP_BUILD_TOOLS "Enable parse tools" OFF)
option(YAML_CPP_BUILD_CONTRIB "Enable contrib stuff in library" OFF)
add_subdirectory(yaml-cpp EXCLUDE_FROM_ALL)

if(USE_DISCORD_RICH_PRESENCE)
	if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/discord_game_sdk.zip")
		message(STATUS "Downloading discord gamesdk...")
		file(DOWNLOAD https://dl-game-sdk.discordapp.net/latest/discord_game_sdk.zip
			"${CMAKE_BINARY_DIR}/external/discord_game_sdk.zip" SHOW_PROGRESS)
	endif()
endif()

if(USE_DISCORD_RICH_PRESENCE)
	if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/discord_game_sdk")
		file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/external/discord_game_sdk")
		execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${CMAKE_BINARY_DIR}/external/discord_game_sdk.zip"
			WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/discord_game_sdk")
		file(RENAME "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/discord_game_sdk.so"
			"${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/libdiscord_game_sdk.so")
	endif()

	add_library(discord-rpc STATIC
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/achievement_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/activity_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/application_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/core.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/image_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/lobby_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/network_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/overlay_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/relationship_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/storage_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/store_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/types.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/user_manager.cpp
		${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp/voice_manager.cpp)

	if(APPLE)
		if(ARCHITECTURE STREQUAL "x86_64")
			target_link_libraries(discord-rpc PUBLIC "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/discord_game_sdk.dylib")
		else()
			target_link_libraries(discord-rpc PUBLIC "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/aarch64/discord_game_sdk.dylib")
		endif()
	elseif(WIN32)
		target_link_libraries(discord-rpc PUBLIC "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/discord_game_sdk.dll.lib")
	elseif(UNIX)
		target_link_libraries(discord-rpc PUBLIC "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/libdiscord_game_sdk.so")
	endif()

	target_include_directories(discord-rpc PUBLIC "${CMAKE_BINARY_DIR}/external/discord_game_sdk/cpp")
endif()

option(BUILD_EXTERNAL "Build external dependencies in /External" OFF)
option(ENABLE_SPVREMAPPER "Enables building of SPVRemapper" OFF)
option(ENABLE_GLSLANG_BINARIES "Builds glslang and spirv-remap" OFF)
option(ENABLE_HLSL "Enables HLSL input support" OFF)
option(GLSLANG_TESTS "Enable glslang testing" OFF)
option(ENABLE_OPT "Enables spirv-opt capability if present" OFF)
add_subdirectory(glslang)

option(SPIRV_CROSS_CLI "Build the CLI binary. Requires SPIRV_CROSS_STATIC." OFF)
option(SPIRV_CROSS_ENABLE_TESTS "Enable SPIRV-Cross tests." OFF)
option(SPIRV_CROSS_ENABLE_HLSL "Enable HLSL target support." OFF)
option(SPIRV_CROSS_ENABLE_MSL "Enable MSL target support." OFF)
option(SPIRV_CROSS_ENABLE_CPP "Enable C++ target support." OFF)
option(SPIRV_CROSS_ENABLE_REFLECT "Enable JSON reflection target support." OFF)
option(SPIRV_CROSS_ENABLE_C_API "Enable C API wrapper support in static library." OFF)
option(SPIRV_CROSS_ENABLE_UTIL "Enable util module support." OFF)
option(SPIRV_CROSS_SKIP_INSTALL "Skips installation targets." ON)
add_subdirectory(SPIRV-Cross)

add_library(dlmalloc STATIC "${CMAKE_CURRENT_SOURCE_DIR}/dlmalloc/dlmalloc.cc")
target_include_directories(dlmalloc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/dlmalloc")
target_compile_definitions(dlmalloc PUBLIC ONLY_MSPACES=1)
target_compile_definitions(dlmalloc PUBLIC USE_LOCK=0)

add_subdirectory(pugixml)

add_library(CLI11 INTERFACE)
target_include_directories(CLI11 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/cli11")

add_library(vulkan INTERFACE)
target_include_directories(vulkan INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator-Hpp/Vulkan-Headers/include")
if(APPLE)
	if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/MoltenVK-macos.tar")
		message(STATUS "Downloading MoltenVK...")
		file(DOWNLOAD https://github.com/KhronosGroup/MoltenVK/releases/download/v1.2.11-artifacts/MoltenVK-macos.tar
			"${CMAKE_BINARY_DIR}/external/MoltenVK-macos.tar" SHOW_PROGRESS)
	endif()
	if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/MoltenVK")
		execute_process(COMMAND tar xf "${CMAKE_BINARY_DIR}/external/MoltenVK-macos.tar" --strip-components=1 MoltenVK/MoltenVK 
			WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external")
	endif()
	target_include_directories(vulkan INTERFACE "${CMAKE_BINARY_DIR}/external/MoltenVK/include")
	set(MOLTENVK_DYLIB "${CMAKE_BINARY_DIR}/external/MoltenVK/dylib/macOS/libMoltenVK.dylib" PARENT_SCOPE)
endif()

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE
	"${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator-Hpp/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator-Hpp/VulkanMemoryAllocator/include")

add_subdirectory(ffmpeg)

if(APPLE OR NOT FORCE_BUILD_OPENSSL_MAC)
	execute_process(
		COMMAND brew --prefix openssl
		RESULT_VARIABLE BREW_RESULT
		OUTPUT_VARIABLE BREW_OPENSSL
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(BREW_RESULT EQUAL 0)
		set(OPENSSL_ROOT_DIR "${BREW_OPENSSL}")
		set(OPENSSL_USE_STATIC_LIBS TRUE)
	endif()
endif()

if(NOT FORCE_BUILD_OPENSSL_MAC)
	find_package(OpenSSL QUIET)
endif()

if(NOT OPENSSL_FOUND OR FORCE_BUILD_OPENSSL_MAC)
	if(MSVC)
		message("OpenSSL not found, using prebuilt version")

		if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/openssl.zip")
			message(STATUS "Downloading openssl...")
			file(DOWNLOAD https://firedaemon.com/download-firedaemon-openssl-3-5-zip
				"${CMAKE_BINARY_DIR}/external/openssl.zip" SHOW_PROGRESS)
		endif()

		if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/openssl")
			file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl")
			execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${CMAKE_BINARY_DIR}/external/openssl.zip"
				WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl")
		endif()

		set(OPENSSL_ROOT_DIR "${CMAKE_BINARY_DIR}/external/openssl/x64" CACHE STRING "Path to OpenSSL root")
	elseif(APPLE)
		message(STATUS "OpenSSL not found.")
		if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/openssl.tar.gz")
			message(STATUS "Downloading OpenSSL@3 from source...")
			file(DOWNLOAD https://github.com/openssl/openssl/releases/download/openssl-3.3.2/openssl-3.3.2.tar.gz
				"${CMAKE_BINARY_DIR}/external/openssl.tar.gz" SHOW_PROGRESS)
		endif()

		if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/openssl")
			file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl")
			execute_process(COMMAND tar xfp "${CMAKE_BINARY_DIR}/external/openssl.tar.gz" -C openssl --strip-components=1
				WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external")
		endif()

		if (NOT EXISTS "${CMAKE_BINARY_DIR}/external/openssl/libssl.a" OR NOT EXISTS "${CMAKE_BINARY_DIR}/external/openssl/libcrypto.a")
			message(STATUS "Building OpenSSL@3...")
			if (ARCHITECTURE STREQUAL "x86_64" OR ARCHITECTURE STREQUAL "arm64")
				execute_process(COMMAND perl Configure darwin64-${ARCHITECTURE} -mmacosx-version-min=11 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl")
				execute_process(COMMAND make -j${CPU_COUNT} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl" OUTPUT_QUIET)
			else()
				if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/openssl/arm64")
					file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl/arm64")
					execute_process(COMMAND perl ../Configure darwin64-arm64 -mmacosx-version-min=11 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl/arm64")
					execute_process(COMMAND make -j${CPU_COUNT} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl/arm64" OUTPUT_QUIET)	
				endif()
				if(NOT EXISTS "${CMAKE_BINARY_DIR}/external/openssl/x86_64")
					file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl/x86_64")
					execute_process(COMMAND perl ../Configure darwin64-x86_64 -mmacosx-version-min=11 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl/x86_64")
					execute_process(COMMAND make -j${CPU_COUNT} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl/x86_64" OUTPUT_QUIET)
				endif()
				execute_process(COMMAND lipo -create arm64/libssl.a x86_64/libssl.a -output libssl.a WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl")
				execute_process(COMMAND lipo -create arm64/libcrypto.a x86_64/libcrypto.a -output libcrypto.a WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/external/openssl")
			endif()
		endif()

		set(OPENSSL_ROOT_DIR "${CMAKE_BINARY_DIR}/external/openssl")
		set(OPENSSL_USE_STATIC_LIBS TRUE)
	endif()
	find_package(OpenSSL REQUIRED)
endif()

message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")

add_library(ssl INTERFACE)
target_link_libraries(ssl INTERFACE OpenSSL::SSL)
add_library(crypto INTERFACE)
target_link_libraries(crypto INTERFACE OpenSSL::Crypto)

# Must be after openssl
add_subdirectory(psvpfstools)

find_package(CURL QUIET)
if(NOT CURL_FOUND)
	message("System curl not found, compiling curl from source")
	include(FetchContent)
	option(BUILD_CURL_EXE "Set to ON to build curl executable." OFF)
	option(CURL_DISABLE_INSTALL "Set to ON to disable installation targets" ON)
	option(HTTP_ONLY "disables all protocols except HTTP (This overrides all CURL_DISABLE_* options)" ON)
	option(BUILD_LIBCURL_DOCS "to build libcurl man pages" OFF)
	option(BUILD_MISC_DOCS "to build misc man pages (e.g. curl-config and mk-ca-bundle)" OFF)
	option(ENABLE_CURL_MANUAL "to build the man page for curl and enable its -M/--manual option" OFF)
	if(WIN32)
		option(CURL_USE_SCHANNEL "Enable Windows native SSL/TLS" ON)
	endif()
	option(CURL_CA_FALLBACK "Set ON to use built-in CA store of TLS backend. Defaults to OFF" ON)
	set(CURL_DISABLE_TESTS ON)
	FetchContent_Declare(curl
		GIT_REPOSITORY https://github.com/curl/curl.git
		GIT_TAG        fd567d4f06857f4fc8e2f64ea727b1318f76ad33 # release 8.8.0
	)
	FetchContent_MakeAvailable(curl)
else()
	add_library(libcurl INTERFACE)
	target_link_libraries(libcurl INTERFACE CURL::libcurl)
endif()


file(GLOB LIBATRAC9_SOURCES
	LibAtrac9/C/src/*.c
	LibAtrac9/C/src/*.h
)
add_library(libatrac9 STATIC ${LIBATRAC9_SOURCES})

target_include_directories(libatrac9 PUBLIC LibAtrac9/C/src)

option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(XXHASH_BUILD_XXHSUM "Build the xxhsum binary" OFF)
option(XXH_X86DISPATCH_ALLOW_AVX "Allow building XXH3 with AVX even if it crashes on SSE2-Only CPUs" OFF)
if(NOT APPLE)
	set(DISPATCH 1)
endif()
add_subdirectory(xxHash/cmake_unofficial EXCLUDE_FROM_ALL)
if(XXH_X86DISPATCH_ALLOW_AVX)
	target_compile_definitions(xxhash PRIVATE XXH_X86DISPATCH_ALLOW_AVX)
endif()

# Tracy
option(TRACY_ENABLE_ON_CORE_COMPONENTS
	"Enable and require Tracy to compile core components such as the renderer, shader recompiler and
		HLE modules"
	ON)
option(TRACY_NO_FRAME_IMAGE, ON)
add_library(tracy STATIC tracy/public/TracyClient.cpp)
target_include_directories(tracy SYSTEM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tracy/public>)
target_link_libraries(tracy PUBLIC ${CMAKE_DL_LIBS})
if(WIN32)
	target_link_libraries(tracy PRIVATE Dbghelp)
endif()

if(UNIX)
	target_link_libraries(tracy PUBLIC pthread)
endif()

# Enable Tracy on-demand profiling mode to avoid unnecessary memory usage when the developer
# isn't profiling by only enabling profiling after a connection has been established with
# a Tracy server
target_compile_definitions(tracy PUBLIC $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_ON_DEMAND>)

# Disable Tracy automated data collection in order to prevent Tracy-related code from being profiled
target_compile_definitions(tracy PUBLIC TRACY_NO_SYSTEM_TRACING)

# Defining TRACY_ENABLE for both compiling Tracy and compiling the project that links
# against it is needed for Tracy to work
#
# -------------------------- Options (choose one at a time) ---------------------------
#
# A (DEFAULT). Enable Tracy on any target that links against if the build configuration
# is either Debug and RelWithDebInfo
#
# Using generator expresions instead of a conditional with CMAKE_BUILD_TYPE is needed
# in order for the condition to work properly on both single-config and multi-config
# CMake project generators. More info here:
# https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#build-configurations
target_compile_definitions(tracy PUBLIC $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_ENABLE>)

#
# ----------------------------------------------------------------------------------------
#
# B. Enable Tracy on every target that links to it regardless of the
# build configuration:
#
# target_compile_definitions(tracy PUBLIC TRACY_ENABLE)

# Use XDG desktop portals on Linux
set(NFD_PORTAL ON)

# nativefiledialog-extended
add_subdirectory(nativefiledialog-extended)

# Create alias to prevent the need of multiple changes in case the target name changes
add_library(NFDe::NFDe ALIAS nfd)
