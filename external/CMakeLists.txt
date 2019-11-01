include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

function(check_submodules_present)
    file(READ "${CMAKE_SOURCE_DIR}/.gitmodules" gitmodules)
    string(REGEX MATCHALL "path *= *[^ \t\r\n]*" gitmodules ${gitmodules})
    foreach(module ${gitmodules})
        string(REGEX REPLACE "path *= *" "" module ${module})
        if (NOT EXISTS "${CMAKE_SOURCE_DIR}/${module}/.git")
            message(FATAL_ERROR "Git submodule ${module} not found. "
                    "Please run: git submodule update --init --recursive")
        endif()
    endforeach()
endfunction()
check_submodules_present()

# Fix a glslang hack
# See https://github.com/KhronosGroup/glslang/issues/1015 and https://github.com/Vita3K/Vita3K/pull/369 for details
if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if(WIN32)
		set(CMAKE_INSTALL_PREFIX "C:/Program Files/${PROJECT_NAME}" CACHE STRING "Default install prefix is: C:/Program Files" FORCE)
	elseif(UNIX)
		set(CMAKE_INSTALL_PREFIX /usr/local CACHE STRING "Default install prefix is: /usr/local" FORCE)
	endif()
endif()

option(CAPSTONE_BUILD_SHARED "Build shared library" OFF)
option(CAPSTONE_BUILD_TESTS "Build tests" OFF)
option(CAPSTONE_ARM64_SUPPORT "ARM64 support" OFF)
option(CAPSTONE_M68K_SUPPORT "M68K support" OFF)
option(CAPSTONE_MIPS_SUPPORT "MIPS support" OFF)
option(CAPSTONE_PPC_SUPPORT "PowerPC support" OFF)
option(CAPSTONE_SPARC_SUPPORT "Sparc support" OFF)
option(CAPSTONE_SYSZ_SUPPORT "SystemZ support" OFF)
option(CAPSTONE_XCORE_SUPPORT "XCore support" OFF)
option(CAPSTONE_X86_SUPPORT "x86 support" OFF)
option(CAPSTONE_TMS320C64X_SUPPORT "TMS320C64x support" OFF)
option(CAPSTONE_M680X_SUPPORT "M680x support" OFF)
option(CAPSTONE_EVM_SUPPORT "EVM support" OFF)
add_subdirectory(capstone EXCLUDE_FROM_ALL)
set(capstone_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/capstone/include" PARENT_SCOPE)

add_library(crypto-algorithms STATIC crypto-algorithms/sha256.c crypto-algorithms/sha256.h)
target_include_directories(crypto-algorithms PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/crypto-algorithms")

add_library(dirent INTERFACE)
if(WIN32)
    target_include_directories(dirent INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/dirent/include")
endif()

add_library(printf INTERFACE)
target_include_directories(printf INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/printf")

add_library(elfio INTERFACE)
target_include_directories(elfio INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/elfio")

add_subdirectory(spdlog EXCLUDE_FROM_ALL)
target_compile_definitions(spdlog INTERFACE SPDLOG_WCHAR_FILENAMES=1 SPDLOG_NO_THREAD_ID=1 SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb")

add_library(glad STATIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/src/glad.c")
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/include")
target_link_libraries(glad PRIVATE ${CMAKE_DL_LIBS})

add_library(better-enums INTERFACE)
target_include_directories(better-enums INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/better-enums")
target_compile_definitions(better-enums INTERFACE BETTER_ENUMS_STRICT_CONVERSION=1)

add_library(googletest STATIC googletest/googletest/src/gtest_main.cc googletest/googletest/src/gtest-all.cc)
target_include_directories(googletest PUBLIC googletest/googletest/include)
target_include_directories(googletest PRIVATE googletest/googletest)
target_compile_definitions(googletest PUBLIC GTEST_HAS_PTHREAD=0)

add_library(microprofile STATIC microprofile/microprofile.cpp microprofile/microprofile.h)
target_include_directories(microprofile PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/microprofile")
set_property(TARGET microprofile PROPERTY CXX_STANDARD 11)
target_compile_definitions(microprofile PUBLIC MICROPROFILE_ENABLED=0 MICROPROFILE_GPU_TIMERS=0)

if(WIN32)
	add_library(nativefiledialog STATIC nativefiledialog-cmake/src/nfd_win.cpp nativefiledialog-cmake/src/nfd_common.c)
	target_include_directories(nativefiledialog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/nativefiledialog-cmake/src/include")
elseif(APPLE)
	add_library(nativefiledialog STATIC nativefiledialog-cmake/src/nfd_cocoa.m nativefiledialog-cmake/src/nfd_common.c)
	target_include_directories(nativefiledialog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/nativefiledialog-cmake/src/include")
elseif(UNIX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

	include_directories(${GTK3_INCLUDE_DIRS})
	link_directories(${GTK3_LIBRARY_DIRS})

	add_definitions(${GTK3_CFLAGS_OTHER})

	add_library(nativefiledialog STATIC nativefiledialog-cmake/src/nfd_gtk.c nativefiledialog-cmake/src/nfd_common.c)
	target_include_directories(nativefiledialog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/nativefiledialog-cmake/src/include")
	target_link_libraries(nativefiledialog ${GTK3_LIBRARIES})
endif()

add_subdirectory(libfat16)

# The imgui target is including both imgui and imgui_club.
add_library(imgui STATIC imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/misc/cpp/imgui_stdlib.cpp)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
		"${CMAKE_CURRENT_SOURCE_DIR}/imgui_club/imgui_memory_editor/")

add_library(miniz STATIC miniz/miniz.c miniz/miniz.h)
target_include_directories(miniz PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/miniz")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/sdl2-cmake-scripts")
if(APPLE)
	set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdl/macos" CACHE PATH "Where SDL2 is located" FORCE)
elseif(WIN32)
	set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdl/windows" CACHE PATH "Where SDL2 is located" FORCE)
	set(SDL2_INCLUDE_DIR "${SDL2_PATH}/include" CACHE PATH "Where the SDL2 include files can be found" FORCE)
endif()
find_package(SDL2 REQUIRED)
add_library(sdl2 INTERFACE)
target_include_directories(sdl2 INTERFACE "${SDL2_INCLUDE_DIR}")
target_link_libraries(sdl2 INTERFACE "${SDL2_LIBRARY}")

if(WIN32)
	add_library(winsock INTERFACE)
	find_library(WSOCK32 wsock32)
	find_library(WS2_32 ws2_32)
	find_library(IPHLPAPI iphlpapi)
	target_link_libraries(winsock INTERFACE WSOCK32 WS2_32 IPHLPAPI)
endif()

add_library(unicorn INTERFACE)
if(APPLE)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/macos/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/macos/lib/libunicorn.a")
elseif(WIN32)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/windows/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/windows/unicorn.lib")
elseif(UNIX)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn-src/include")
	externalproject_add(
		unicorn_build
		SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/unicorn-src"
		BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/unicorn-src"
		CONFIGURE_COMMAND ""
		BUILD_COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/unicorn-build.sh"
		INSTALL_COMMAND "")
	add_dependencies(unicorn unicorn_build)
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn-src/libunicorn.a")
else()
	find_package(PkgConfig)
	pkg_check_modules(UNICORN REQUIRED unicorn)
	target_include_directories(unicorn INTERFACE "${UNICORN_INCLUDE_DIRS}")
	find_library(UNICORN_LIB NAMES "${UNICORN_STATIC_LIBRARIES}" PATHS "${UNICORN_LIBRARY_DIRS}")
	target_link_libraries(unicorn INTERFACE "${UNICORN_LIB}")
endif()

add_library(vita-headers INTERFACE)
target_include_directories(vita-headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vita-headers/include")

add_library(vita-toolchain INTERFACE)
target_include_directories(vita-toolchain INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vita-toolchain/src")

option(YAML_CPP_BUILD_TESTS "Enable testing" OFF)
option(YAML_CPP_BUILD_TOOLS "Enable parse tools" OFF)
option(YAML_CPP_BUILD_CONTRIB "Enable contrib stuff in library" OFF)
option(YAML_CPP_INSTALL "Enable generation of install target" OFF)
add_subdirectory(yaml-cpp EXCLUDE_FROM_ALL)
add_library(yaml INTERFACE)
target_include_directories(yaml INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/include")
target_link_libraries(yaml INTERFACE yaml-cpp)

add_library(rpcs3 INTERFACE)
target_include_directories(rpcs3 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/rpcs3/include")

add_library(discord-rpc INTERFACE)
if (USE_DISCORD_RICH_PRESENCE)
	target_compile_definitions(discord-rpc INTERFACE DISCORD_RPC=NDEBUG)
endif()
target_include_directories(discord-rpc INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/discord-rpc/include")
if(APPLE)
	target_link_libraries(discord-rpc INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/discord-rpc-lib/libdiscord-rpc-mac.a")
elseif(WIN32)
	target_link_libraries(discord-rpc INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/discord-rpc-lib/discord-rpc.lib")
elseif(UNIX)
	target_link_libraries(discord-rpc INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/discord-rpc-lib/libdiscord-rpc-linux.a")
endif()

option(ENABLE_SPVREMAPPER "Enables building of SPVRemapper" OFF)
option(ENABLE_GLSLANG_BINARIES "Builds glslangValidator and spirv-remap" OFF)
option(ENABLE_AMD_EXTENSIONS "Enables support of AMD-specific extensions" OFF)
option(ENABLE_NV_EXTENSIONS "Enables support of Nvidia-specific extensions" OFF)
option(ENABLE_HLSL "Enables HLSL input support" OFF)
add_subdirectory(glslang)
target_include_directories(glslang INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/glslang/SPIRV")

option(SPIRV_CROSS_CLI "Build the CLI binary. Requires SPIRV_CROSS_STATIC." OFF)
option(SPIRV_CROSS_ENABLE_TESTS "Enable SPIRV-Cross tests." OFF)
option(SPIRV_CROSS_ENABLE_HLSL "Enable HLSL target support." OFF)
option(SPIRV_CROSS_ENABLE_MSL "Enable MSL target support." OFF)
option(SPIRV_CROSS_ENABLE_CPP "Enable C++ target support." OFF)
option(SPIRV_CROSS_ENABLE_REFLECT "Enable JSON reflection target support." OFF)
option(SPIRV_CROSS_ENABLE_C_API "Enable C API wrapper support in static library." OFF)
option(SPIRV_CROSS_ENABLE_UTIL "Enable util module support." OFF)
add_subdirectory(SPIRV-Cross)

add_library(dlmalloc STATIC "${CMAKE_CURRENT_SOURCE_DIR}/dlmalloc/dlmalloc.cc")
target_include_directories(dlmalloc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/dlmalloc")
target_compile_definitions(dlmalloc PUBLIC ONLY_MSPACES=1)

add_subdirectory(pugixml)

if (USE_VULKAN)
	find_package(Vulkan REQUIRED)
	add_library(vulkan INTERFACE)
	target_include_directories(vulkan INTERFACE ${Vulkan_INCLUDE_DIRS})
	target_link_libraries(vulkan INTERFACE ${Vulkan_LIBRARIES})

	add_library(vma INTERFACE)
	target_include_directories(vma INTERFACE VulkanMemoryAllocator/src)
endif()

add_subdirectory(ffmpeg)