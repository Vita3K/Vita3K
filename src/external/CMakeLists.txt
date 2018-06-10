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

# capstone
option(CAPSTONE_BUILD_SHARED "Build shared library" OFF)
option(CAPSTONE_BUILD_TESTS "Build tests" OFF)
option(CAPSTONE_ARM_SUPPORT "ARM support" ON)
option(CAPSTONE_ARM64_SUPPORT "ARM64 support" OFF)
option(CAPSTONE_MIPS_SUPPORT "MIPS support" OFF)
option(CAPSTONE_PPC_SUPPORT "PowerPC support" OFF)
option(CAPSTONE_SPARC_SUPPORT "Sparc support" OFF)
option(CAPSTONE_SYSZ_SUPPORT "SystemZ support" OFF)
option(CAPSTONE_XCORE_SUPPORT "XCore support" OFF)
option(CAPSTONE_X86_SUPPORT "x86 support" OFF)
add_subdirectory(capstone EXCLUDE_FROM_ALL)
set(capstone_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/capstone/include" PARENT_SCOPE)

# crypto-algorithms
add_library(crypto-algorithms STATIC crypto-algorithms/sha256.c crypto-algorithms/sha256.h)
target_include_directories(crypto-algorithms PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/crypto-algorithms")

# dirent
add_library(dirent INTERFACE)
if(WIN32)
    target_include_directories(dirent INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/dirent/include")
endif()

# elfio
add_library(elfio INTERFACE)
target_include_directories(elfio INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/elfio")

# spdlog
add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include")
target_compile_definitions(spdlog INTERFACE SPDLOG_WCHAR_FILENAMES=1 SPDLOG_NO_THREAD_ID=1)

# glbinding
option(BUILD_SHARED_LIBS "Build shared instead of static libraries." OFF)
option(OPTION_BUILD_TESTS "Build tests." OFF)
option(OPTION_BUILD_GPU_TESTS "Build tests that require an OpenGL context." OFF)
option(OPTION_BUILD_TOOLS "Build tools." OFF)
add_subdirectory(glbinding EXCLUDE_FROM_ALL)

# googletest
add_library(googletest STATIC googletest/googletest/src/gtest_main.cc googletest/googletest/src/gtest-all.cc)
target_include_directories(googletest PUBLIC googletest/googletest/include)
target_include_directories(googletest PRIVATE googletest/googletest)
target_compile_definitions(googletest PUBLIC GTEST_HAS_PTHREAD=0)

# microprofile
add_library(microprofile STATIC microprofile/microprofile.cpp microprofile/microprofile.h)
target_include_directories(microprofile PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/microprofile")
set_property(TARGET microprofile PROPERTY CXX_STANDARD 11)
target_compile_definitions(microprofile PUBLIC MICROPROFILE_ENABLED=0 MICROPROFILE_GPU_TIMERS=0)

# imgui
add_library(imgui STATIC imgui/imgui.cpp imgui/imgui_draw.cpp)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

# miniz
add_library(miniz STATIC miniz/miniz.c miniz/miniz.h)
target_include_directories(miniz PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/miniz")

# sdl2
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/sdl2-cmake-scripts")
if(APPLE)
	set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdl/macos")
elseif(WIN32)
	set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdl/windows")
endif()
find_package(SDL2 REQUIRED)
add_library(sdl2 INTERFACE)
target_include_directories(sdl2 INTERFACE "${SDL2_INCLUDE_DIR}")
target_link_libraries(sdl2 INTERFACE "${SDL2_LIBRARY}")

# winsock
if(WIN32)
	add_library(winsock INTERFACE)
	find_library(WSOCK32 wsock32)
	find_library(WS2_32 ws2_32)
	target_link_libraries(winsock INTERFACE WSOCK32 WS2_32)
endif()

# unicorn
add_library(unicorn INTERFACE)
if(APPLE)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/macos/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/macos/lib/libunicorn.a")
elseif(WIN32)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/windows/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/windows/unicorn.lib")
elseif(UNIX)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/unix/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/unix/lib/libunicorn.a")
else()
	find_package(PkgConfig)
	pkg_check_modules(UNICORN REQUIRED unicorn)
	target_include_directories(unicorn INTERFACE "${UNICORN_INCLUDE_DIRS}")
	find_library(UNICORN_LIB NAMES "${UNICORN_STATIC_LIBRARIES}" PATHS "${UNICORN_LIBRARY_DIRS}")
	target_link_libraries(unicorn INTERFACE "${UNICORN_LIB}")
endif()

# vita-headers
add_library(vita-headers INTERFACE)
target_include_directories(vita-headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vita-headers/include")

# vita-toolchain
add_library(vita-toolchain INTERFACE)
target_include_directories(vita-toolchain INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vita-toolchain/src")

# yaml-cpp
option(YAML_CPP_BUILD_TOOLS "Enable testing and parse tools" OFF)
option(YAML_CPP_BUILD_CONTRIB "Enable contrib stuff in library" OFF)
add_subdirectory(yaml-cpp EXCLUDE_FROM_ALL)
add_library(yaml INTERFACE)
target_include_directories(yaml INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/include")
target_link_libraries(yaml INTERFACE yaml-cpp)

# rpcs3
add_library(rpcs3 INTERFACE)
target_include_directories(rpcs3 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/rpcs3/include")
