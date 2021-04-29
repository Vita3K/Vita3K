set(VITA3K_ORG_NAME "Vita3K")
set(VITA3K_APP_NAME "Vita3K")
set(VITA3K_APP_VERSION "v0.1.2")

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

macro(configure_boost)
	message("configuring boost")

	set(Boost_USE_STATIC_LIBS ON)
	set(Boost_USE_MULTITHREADED ON)
	set(BOOST_COMPONENTS filesystem system)

	find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)

	include_directories(${Boost_INCLUDE_DIRS})

	message("Using Boost_VERSION: ${Boost_VERSION}")
	message("Using Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
	message("Using Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
endmacro(configure_boost)

configure_boost()
if(${STANDARD_NUM} GREATER 14)
	add_definitions(-DVITA3K_CPP${STANDARD_NUM})
endif()

if (USE_DISCORD_RICH_PRESENCE)
	add_definitions(-DUSE_DISCORD)
endif()

if (USE_VULKAN)
	add_definitions(-DUSE_VULKAN)
endif()

add_subdirectory(app)
add_subdirectory(audio)
add_subdirectory(config)
add_subdirectory(cpu)
add_subdirectory(crypto)
add_subdirectory(ctrl)
add_subdirectory(codec)
add_subdirectory(dialog)
add_subdirectory(disasm)
add_subdirectory(features)
add_subdirectory(glutil)
add_subdirectory(gui)
add_subdirectory(gxm)
add_subdirectory(ime)
add_subdirectory(net)
add_subdirectory(ngs)
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
add_subdirectory(threads)
add_subdirectory(touch)
add_subdirectory(util)
add_subdirectory(gdbstub)

add_executable(vita3k MACOSX_BUNDLE main.cpp interface.cpp interface.h performance.cpp)

target_link_libraries(vita3k PRIVATE app gui modules)
if(USE_DISCORD_RICH_PRESENCE)
	target_link_libraries(vita3k PRIVATE discord-rpc)
endif()
if(LINUX)
	target_link_libraries(vita3k PRIVATE stdc++fs)
endif()
if(${STANDARD_NUM} GREATER 14)
	target_compile_features(vita3k PUBLIC cxx_std_${STANDARD_NUM})
endif()

set_target_properties(vita3k PROPERTIES OUTPUT_NAME Vita3K
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
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "$<TARGET_FILE_DIR:vita3k>/../Resources/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../lang" "$<TARGET_FILE_DIR:vita3k>/../Resources/lang"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin/" "$<TARGET_FILE_DIR:vita3k>/../Resources/shaders-builtin"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/external/sdl/macos/SDL2.framework" "$<TARGET_FILE_DIR:vita3k>/../Frameworks/SDL2.framework")
	if(USE_DISCORD_RICH_PRESENCE)
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/discord_game_sdk.dylib" "$<TARGET_FILE_DIR:vita3k>/../Resources")
	endif()
	set_target_properties(vita3k PROPERTIES LINK_FLAGS "-rpath @loader_path/../Frameworks/ -rpath @loader_path/../Resources/")
	target_sources(vita3k PRIVATE Vita3K.icns)
	set(MACOSX_BUNDLE_ICON_FILE Vita3K.icns)
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.Vita3K.Vita3K")
	set(MACOSX_BUNDLE_BUNDLE_NAME "Vita3K")
elseif(LINUX)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")
	add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "$<TARGET_FILE_DIR:vita3k>/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../lang" "$<TARGET_FILE_DIR:vita3k>/lang"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "$<TARGET_FILE_DIR:vita3k>/shaders-builtin")
	if(USE_DISCORD_RICH_PRESENCE)
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/libdiscord_game_sdk.so" "$<TARGET_FILE_DIR:vita3k>")
	endif()
elseif(WIN32)
	target_sources(vita3k PRIVATE resource.h Vita3K.ico Vita3K.rc WindowsDpiAwareness.manifest)
	set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT vita3k)
	set_target_properties(vita3k PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build-windows/bin")
	add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "$<TARGET_FILE_DIR:vita3k>/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../lang" "$<TARGET_FILE_DIR:vita3k>/lang"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "$<TARGET_FILE_DIR:vita3k>/shaders-builtin"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/external/sdl/windows/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:vita3k>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${UNICORN_DLL_DIR}/unicorn.dll" "$<TARGET_FILE_DIR:vita3k>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${UNICORN_DLL_DIR}/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:vita3k>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${UNICORN_DLL_DIR}/libwinpthread-1.dll" "$<TARGET_FILE_DIR:vita3k>")
	if(USE_DISCORD_RICH_PRESENCE)
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/discord_game_sdk.dll" "$<TARGET_FILE_DIR:vita3k>")
	endif()
endif()
