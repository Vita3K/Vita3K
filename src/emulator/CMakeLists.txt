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

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(LINUX TRUE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

macro(configure_boost)
	message("configuring boost")

	set(Boost_USE_STATIC_LIBS ON)
	set(Boost_USE_MULTITHREADED ON)
	set(BOOST_COMPONENTS filesystem program_options system)

	find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)

	include_directories(${Boost_INCLUDE_DIRS})

	if(Boost_FOUND)
		message("Using Boost_VERSION: ${Boost_VERSION}")
		message("Using Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
		message("Using Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
	else()
		message("Boost library is not found")
	endif()
endmacro(configure_boost)

configure_boost()

add_subdirectory(audio)
add_subdirectory(cpu)
add_subdirectory(crypto)
add_subdirectory(ctrl)
add_subdirectory(dialog)
add_subdirectory(disasm)
add_subdirectory(glutil)
add_subdirectory(gxm)
add_subdirectory(net)
add_subdirectory(np)
add_subdirectory(host)
add_subdirectory(io)
add_subdirectory(kernel)
add_subdirectory(mem)
add_subdirectory(module)
add_subdirectory(modules)
add_subdirectory(nids)
add_subdirectory(renderer)
add_subdirectory(rtc)
add_subdirectory(shader)
add_subdirectory(util)
add_subdirectory(threads)
add_subdirectory(gui)

add_executable(
emulator
MACOSX_BUNDLE
main.cpp
)

if(LINUX)
	target_link_libraries(emulator PRIVATE ${Boost_LIBRARIES} cpu dialog elfio glutil gui host imgui modules nids vita-toolchain util stdc++fs)
else()
	target_link_libraries(emulator PRIVATE ${Boost_LIBRARIES} cpu dialog elfio glutil gui host imgui modules nids vita-toolchain util)
endif()
set_target_properties(emulator PROPERTIES OUTPUT_NAME Vita3K
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(APPLE)
	add_custom_command(
		OUTPUT Vita3K.icns
		COMMAND mkdir -p Vita3K.iconset
		COMMAND sips -z 16 16     "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_16x16.png
		COMMAND sips -z 32 32     "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_16x16@2x.png
		COMMAND sips -z 32 32     "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_32x32.png
		COMMAND sips -z 64 64     "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_32x32@2x.png
		COMMAND sips -z 128 128   "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_128x128.png
		COMMAND sips -z 256 256   "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_128x128@2x.png
		COMMAND sips -z 256 256   "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_256x256.png
		COMMAND sips -z 512 512   "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_256x256@2x.png
		COMMAND sips -z 512 512   "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" --out Vita3K.iconset/icon_512x512.png
		COMMAND cp "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png" Vita3K.iconset/icon_512x512@2x.png
		COMMAND iconutil -c icns Vita3K.iconset
		COMMAND rm -r Vita3K.iconset
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K.png")
	set_source_files_properties(Vita3K.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

	add_custom_command(
		TARGET emulator
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../../data" "$<TARGET_FILE_DIR:emulator>/../Resources/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-db/shaders" "$<TARGET_FILE_DIR:emulator>/../Resources/shaders"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin/" "$<TARGET_FILE_DIR:emulator>/../Resources/shaders-builtin"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/external/sdl/macos/SDL2.framework" "$<TARGET_FILE_DIR:emulator>/../Frameworks/SDL2.framework")
	set_target_properties(emulator PROPERTIES LINK_FLAGS "-rpath @executable_path/../Frameworks/")
	target_sources(emulator PRIVATE Vita3K.icns)
	set(MACOSX_BUNDLE_ICON_FILE Vita3K.icns)
elseif(LINUX)
	add_custom_command(
		TARGET emulator
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../../data" "$<TARGET_FILE_DIR:emulator>/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-db/shaders" "$<TARGET_FILE_DIR:emulator>/shaders"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "$<TARGET_FILE_DIR:emulator>/shaders-builtin")
elseif(WIN32)
	target_sources(emulator PRIVATE resource.h Vita3K.ico Vita3K.rc)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT emulator)
	set_target_properties(emulator PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build-windows/bin")
	add_custom_command(
		TARGET emulator
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../../data" "$<TARGET_FILE_DIR:emulator>/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-db/shaders" "$<TARGET_FILE_DIR:emulator>/shaders"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "$<TARGET_FILE_DIR:emulator>/shaders-builtin"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/sdl/windows/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/unicorn.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:emulator>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libwinpthread-1.dll" "$<TARGET_FILE_DIR:emulator>")
endif()
