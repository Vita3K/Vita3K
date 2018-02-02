set(VITA3K_ORG_NAME "Vita3K")
set(VITA3K_APP_NAME "Vita3K")
set(VITA3K_VERSION $ENV{VITA3K_VERSION})

if(VITA3K_VERSION STREQUAL "")
	execute_process(
		COMMAND git log -1 --format=%H
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(GIT_HASH STREQUAL "")
		set(VITA3K_VERSION "development version")
	else()
		set(VITA3K_VERSION "(git ${GIT_HASH})")
	endif()
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(audio)
add_subdirectory(cpu)
add_subdirectory(ctrl)
add_subdirectory(disasm)
add_subdirectory(glutil)
add_subdirectory(gxm)
add_subdirectory(host)
add_subdirectory(io)
add_subdirectory(kernel)
add_subdirectory(mem)
add_subdirectory(module)
add_subdirectory(modules)
add_subdirectory(nids)
add_subdirectory(util)

add_executable(
emulator
MACOSX_BUNDLE
shaders
load_self.cpp
load_self.h
main.cpp
relocation.cpp
relocation.h
vpk.cpp
vpk.h
)

target_link_libraries(emulator PRIVATE cpu elfio host modules nids vita-toolchain)
set_target_properties(emulator PROPERTIES
	OUTPUT_NAME Vita3K
	RESOURCE shaders)

if(WIN32)
	add_custom_command(
		TARGET emulator
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:emulator>/shaders"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/sdl/windows/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/unicorn.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libwinpthread-1.dll" "$<TARGET_FILE_DIR:emulator>")
endif()
