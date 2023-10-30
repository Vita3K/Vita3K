set(VITA3K_ORG_NAME "Vita3K")
set(VITA3K_APP_NAME "Vita3K")
set(VITA3K_APP_VER_HI 0)
set(VITA3K_APP_VER_MID 1)
set(VITA3K_APP_VER_LO 9)
set(VITA3K_APP_VERSION "v${VITA3K_APP_VER_HI}.${VITA3K_APP_VER_MID}.${VITA3K_APP_VER_LO}")

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

# Value is different when is PR
if("$ENV{GITHUB_EVENT_NAME}" STREQUAL "pull_request")
	set(GIT_BRANCH "$ENV{GITHUB_HEAD_REF}")
	if(WIN32)
		set(CMD powershell "(Get-Content $ENV{GITHUB_EVENT_PATH} | ConvertFrom-Json).pull_request.head.repo.html_url")
	else()
		set(CMD bash -c "jq .pull_request.head.repo.html_url $ENV{GITHUB_EVENT_PATH} | tr -d \\\" ")
	endif()
else() 
	execute_process(
		COMMAND git rev-parse --abbrev-ref HEAD
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	set(CMD git config --get remote.origin.url)
endif()

execute_process(COMMAND ${CMD}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_REPO
	OUTPUT_STRIP_TRAILING_WHITESPACE)

if (NOT GIT_REPO STREQUAL "")
	string(FIND ${GIT_REPO} "git@github.com:" IS_GIT_SSH)
	if (IS_GIT_SSH EQUAL 0)
		string(REPLACE "git@github.com:" "" GIT_REPO ${GIT_REPO})
	else()
		string(REPLACE "https://github.com/" "" GIT_REPO ${GIT_REPO})
	endif()
	string(REPLACE "/" ";" GIT_REPO ${GIT_REPO})
	list(POP_BACK GIT_REPO)
endif()

if(GIT_HASH STREQUAL "")
	set(VITA3K_GIT_REV "Development version")
	set(GIT_COUNT 0)
elseif((GIT_REPO STREQUAL "Vita3K") AND ((GIT_BRANCH STREQUAL "master") OR (GIT_BRANCH STREQUAL "HEAD")))
	set(VITA3K_GIT_REV "${GIT_HASH}")

	if(USE_VITA3K_UPDATE)
		add_definitions(-DUSE_VITA3K_UPDATE)
	endif()
else()
	set(VITA3K_GIT_REV "${GIT_HASH}-${GIT_REPO}/${GIT_BRANCH}")
endif()

file(WRITE ${CMAKE_BINARY_DIR}/vita3k/config/gitver.h
	"#define APP_VER_HI ${VITA3K_APP_VER_HI}\n"
	"#define APP_VER_MID ${VITA3K_APP_VER_MID}\n"
	"#define APP_VER_LO ${VITA3K_APP_VER_LO}\n"
	"#define APP_NUMBER ${GIT_COUNT}\n"
)

include_directories(${CMAKE_BINARY_DIR}/vita3k/config)

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

if (USE_DISCORD_RICH_PRESENCE)
	add_definitions(-DUSE_DISCORD)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT MSVC)
	add_compile_options(-Wformat -Werror=format-security)
endif()

add_subdirectory(app)
add_subdirectory(audio)
add_subdirectory(config)
add_subdirectory(cpu)
add_subdirectory(crypto)
add_subdirectory(ctrl)
add_subdirectory(codec)
add_subdirectory(compat)
add_subdirectory(dialog)
add_subdirectory(display)
add_subdirectory(features)
add_subdirectory(glutil)
add_subdirectory(gui)
add_subdirectory(gxm)
add_subdirectory(host)
add_subdirectory(ime)
add_subdirectory(lang)
add_subdirectory(net)
add_subdirectory(ngs)
add_subdirectory(np)
add_subdirectory(emuenv)
add_subdirectory(http)
add_subdirectory(io)
add_subdirectory(kernel)
add_subdirectory(mem)
add_subdirectory(module)
add_subdirectory(modules)
add_subdirectory(motion)
add_subdirectory(nids)
add_subdirectory(regmgr)
add_subdirectory(renderer)
add_subdirectory(rtc)
add_subdirectory(shader)
add_subdirectory(threads)
add_subdirectory(touch)
add_subdirectory(util)
add_subdirectory(gdbstub)
add_subdirectory(packages)
add_subdirectory(vkutil)

add_executable(vita3k MACOSX_BUNDLE main.cpp interface.cpp interface.h performance.cpp)

target_link_libraries(vita3k PRIVATE app config ctrl display gdbstub gui gxm io miniz modules packages renderer shader touch)
if(USE_DISCORD_RICH_PRESENCE)
	target_link_libraries(vita3k PRIVATE discord-rpc)
endif()

if(LINUX)
	target_link_libraries(vita3k PRIVATE -static-libgcc -static-libstdc++)
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
	if(USE_VITA3K_UPDATE)
		add_custom_command(
			TARGET vita3k
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/script/update-macos.sh" "$<TARGET_FILE_DIR:vita3k>/../Resources/update-vita3k.sh")
	endif()
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

	# taken from https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DolphinQt/CMakeLists.txt
	find_file(MOLTENVK_DYLIB NAMES libMoltenVK.dylib PATH_SUFFIXES lib)
	if(NOT MOLTENVK_DYLIB)
		message(FATAL_ERROR "Couldn't find libMoltenVK.dylib.")
	else()
		message("Found Molten-VK dylib: ${MOLTENVK_DYLIB}")
	endif()
	target_sources(vita3k PRIVATE ${MOLTENVK_DYLIB})
	set_source_files_properties(${MOLTENVK_DYLIB} PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks)
elseif(LINUX)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")
	add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "$<TARGET_FILE_DIR:vita3k>/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../lang" "$<TARGET_FILE_DIR:vita3k>/lang"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "$<TARGET_FILE_DIR:vita3k>/shaders-builtin")
	if(USE_VITA3K_UPDATE)
		add_custom_command(
			TARGET vita3k
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/script/update-linux.sh" "$<TARGET_FILE_DIR:vita3k>/update-vita3k.sh")
	endif()
	if(USE_DISCORD_RICH_PRESENCE)
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/libdiscord_game_sdk.so" "$<TARGET_FILE_DIR:vita3k>")
	endif()
elseif(WIN32)
	target_sources(vita3k PRIVATE resource.h Vita3K.ico Vita3K.rc Windows.manifest)
	set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT vita3k)
	set_target_properties(vita3k PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$(Configuration)")
	add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "$<TARGET_FILE_DIR:vita3k>/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../lang" "$<TARGET_FILE_DIR:vita3k>/lang"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "$<TARGET_FILE_DIR:vita3k>/shaders-builtin"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/external/sdl/windows/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:vita3k>")
	if(USE_VITA3K_UPDATE)
		add_custom_command(
			TARGET vita3k
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/script/update-windows.bat" "$<TARGET_FILE_DIR:vita3k>/update-vita3k.bat")
	endif()
	if(USE_DISCORD_RICH_PRESENCE)
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/discord_game_sdk.dll" "$<TARGET_FILE_DIR:vita3k>")
	endif()
	if(EXISTS "${CMAKE_BINARY_DIR}/external/openssl")
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/openssl/openssl-3/x64/bin/libssl-3-x64.dll" "$<TARGET_FILE_DIR:vita3k>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/openssl/openssl-3/x64/bin/libcrypto-3-x64.dll" "$<TARGET_FILE_DIR:vita3k>")
	endif()
endif()

# Enable Tracy profiling on the main compilation target
target_link_libraries(vita3k PRIVATE tracy)
