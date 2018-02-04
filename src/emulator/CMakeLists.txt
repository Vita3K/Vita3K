set(VITA3K_ORG_NAME "Vita3K")
set(VITA3K_APP_NAME "Vita3K")
set(VITA3K_APP_VERSION "v0.1")

execute_process(
	COMMAND git rev-list HEAD --count
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_COUNT
	OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
	COMMAND git rev-parse --short HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE)
	
if(GIT_HASH STREQUAL "")
	set(VITA3K_GIT_REV "Development version")
else()
	set(VITA3K_GIT_REV "${GIT_COUNT}-${GIT_HASH}")
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
load_self.cpp
load_self.h
main.cpp
relocation.cpp
relocation.h
resource.h
VIta3K.ico
Vita3K.rc
vpk.cpp
vpk.h
)

target_link_libraries(emulator PRIVATE cpu elfio host modules nids vita-toolchain)
set_target_properties(emulator PROPERTIES OUTPUT_NAME Vita3K
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(APPLE)
	add_custom_command(
		TARGET emulator
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:emulator>/../Resources/shaders"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/external/sdl/macos/SDL2.framework" "$<TARGET_FILE_DIR:emulator>/../Frameworks/SDL2.framework")
    set_target_properties(emulator PROPERTIES LINK_FLAGS "-rpath @executable_path/../Frameworks/")
elseif(WIN32)
	add_custom_command(
		TARGET emulator
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:emulator>/shaders"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/sdl/windows/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/unicorn.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libwinpthread-1.dll" "$<TARGET_FILE_DIR:emulator>")
endif()
