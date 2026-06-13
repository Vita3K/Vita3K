set(VITA3K_ORG_NAME "Vita3K")
set(VITA3K_APP_NAME "Vita3K")
set(VITA3K_APP_VER_HI 0)
set(VITA3K_APP_VER_MID 2)
set(VITA3K_APP_VER_LO 1)
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

set(VITA3K_OFFICIAL_UPDATE_BUILD 0)

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
	set(VITA3K_OFFICIAL_UPDATE_BUILD 1)
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

	find_package(X11)
	if(X11_FOUND)
		add_compile_definitions(HAVE_X11)
	endif()

	find_package(PkgConfig)
	if(PkgConfig_FOUND)
		pkg_check_modules(WAYLAND wayland-client)
		if(WAYLAND_FOUND)
			add_compile_definitions(HAVE_WAYLAND)
		endif()
	endif()
endif()

get_boost()

include_directories(${Boost_INCLUDE_DIRS})

if (USE_DISCORD_RICH_PRESENCE)
	add_definitions(-DUSE_DISCORD)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT MSVC)
	add_compile_options(-Wformat -Werror=format-security)
endif()

if(NOT ANDROID)
	include(${CMAKE_SOURCE_DIR}/cmake/qt6.cmake)
endif()

add_subdirectory(app)
add_subdirectory(audio)
add_subdirectory(camera)
add_subdirectory(input)
add_subdirectory(config)
add_subdirectory(cpu)
add_subdirectory(ctrl)
add_subdirectory(codec)
add_subdirectory(compat)
add_subdirectory(dialog)
add_subdirectory(display)
add_subdirectory(features)
add_subdirectory(glutil)
add_subdirectory(updater)
if(NOT ANDROID)
	add_subdirectory(gui-qt)
endif()
add_subdirectory(gxm)
add_subdirectory(ime)
add_subdirectory(net)
add_subdirectory(ngs)
add_subdirectory(np)
add_subdirectory(emuenv)
add_subdirectory(http)
add_subdirectory(io)
add_subdirectory(kernel)
add_subdirectory(lang)
add_subdirectory(mem)
add_subdirectory(patch)
add_subdirectory(module)
add_subdirectory(modules)
add_subdirectory(motion)
add_subdirectory(nids)
add_subdirectory(overlay)
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

if(ANDROID)
	add_library(vita3k SHARED
		android/jni/android_state.cpp
		android/jni/filesystem_android.cpp
		android/jni/main_android.cpp
		android/jni/native_bootstrap.cpp
		android/jni/native_config.cpp
		android/jni/native_drivers.cpp
		android/jni/native_users.cpp
		android/jni/native_apps.cpp
		android/jni/native_trophies.cpp
		android/jni/native_install.cpp
		android/jni/native_session.cpp
		android/jni/input_overlay.cpp
		android/jni/ime.cpp
		interface.cpp
		performance.cpp
	)
	target_include_directories(vita3k PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(vita3k PRIVATE SDL3::SDL3 pugixml::pugixml android)

	if(CMAKE_BUILD_TYPE STREQUAL "Release")
		# we need to exclude all symbols except SDL
		set(LIBS_TO_EXCLUDE
			"libboost_atomic.a"
			"libboost_container.a"
			"libboost_date_time.a"
			"libboost_exception.a"
			"libboost_filesystem.a"
			"libboost_program_options.a"
			"libboost_regex.a"
			"libboost_system.a"
			"libssl.a"
			"libfmt.a"
			"libcrypto.a"
			"libavcodec.a"
			"libavdevice.a"
			"libavfilter.a"
			"libavformat.a"
			"libavutil.a"
			"libswresample.a"
			"libswscale.a"
			)
		set(LIBS_TO_EXCLUDE_FLAG "")
		foreach(lib IN LISTS LIBS_TO_EXCLUDE)
			string(APPEND LIBS_TO_EXCLUDE_FLAG " -Wl,--exclude-libs,${lib}")
		endforeach()
		set_target_properties(vita3k PROPERTIES LINK_FLAGS "${LIBS_TO_EXCLUDE_FLAG}")
	endif()
else()
	add_executable(vita3k MACOSX_BUNDLE main.cpp interface.cpp interface.h performance.cpp)
endif()

if(WIN32)
	# This file is a bit special, it is run before anything else and must be added here
	target_sources(vita3k PRIVATE util/src/vc_runtime_checker.cpp)
endif()

target_link_libraries(vita3k PRIVATE app audio compat config cppcommon ctrl dialog display ime lang gdbstub gxm io miniz modules motion packages patch renderer shader touch util psvpfsparser)
if(NOT ANDROID)
	target_link_libraries(vita3k PRIVATE gui-qt)
endif()
if(USE_DISCORD_RICH_PRESENCE)
	target_link_libraries(vita3k PRIVATE discord-rpc)
endif()

if(NOT ANDROID)
	file(GLOB VITA3K_QT_TS_FILES CONFIGURE_DEPENDS
		"${CMAKE_SOURCE_DIR}/i18n/qt/*.ts")
	set(VITA3K_QT_QM_DIR "${CMAKE_BINARY_DIR}/qt_translations")
	set(VITA3K_QT_QM_FILES "")

	foreach(ts_file IN LISTS VITA3K_QT_TS_FILES)
		get_filename_component(ts_name "${ts_file}" NAME_WE)
		set(qm_file "${VITA3K_QT_QM_DIR}/${ts_name}.qm")
		list(APPEND VITA3K_QT_QM_FILES "${qm_file}")
		add_custom_command(
			OUTPUT "${qm_file}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${VITA3K_QT_QM_DIR}"
			COMMAND $<TARGET_FILE:Qt6::lrelease> "${ts_file}" -qm "${qm_file}"
			DEPENDS "${ts_file}"
			VERBATIM)
	endforeach()

	add_custom_target(qt_app_translations DEPENDS ${VITA3K_QT_QM_FILES})
	add_dependencies(vita3k qt_app_translations)

	add_custom_target(update_qt_translations
		COMMAND $<TARGET_FILE:Qt6::lupdate>
			-no-obsolete
			-recursive
			"${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
			"${CMAKE_CURRENT_SOURCE_DIR}/gui-qt/src"
			"${CMAKE_CURRENT_SOURCE_DIR}/gui-qt/include"
			-ts "${CMAKE_SOURCE_DIR}/i18n/qt/vita3k_en.ts"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		COMMENT "Updating Qt translation source file")
endif()

set_target_properties(vita3k PROPERTIES OUTPUT_NAME Vita3K)

if(NOT ANDROID)
	set_target_properties(vita3k PROPERTIES
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

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
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${VITA3K_QT_QM_DIR}" "$<TARGET_FILE_DIR:vita3k>/../Resources/translations"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin/" "$<TARGET_FILE_DIR:vita3k>/../Resources/shaders-builtin")
	if(USE_DISCORD_RICH_PRESENCE)
		if(ARCHITECTURE STREQUAL "x86_64")
			set(discord_dylib "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/discord_game_sdk.dylib")
		elseif(ARCHITECTURE STREQUAL "arm64")
			set(discord_dylib "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/aarch64/discord_game_sdk.dylib")
		else() # macos universal build
			set(discord_dylib "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/discord_game_sdk.dylib")
		endif()

		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${discord_dylib}" "$<TARGET_FILE_DIR:vita3k>/../Resources")
	endif()
	set_target_properties(vita3k PROPERTIES LINK_FLAGS "-rpath @loader_path/../Frameworks/ -rpath @loader_path/../Resources/ -ld64")
	target_sources(vita3k PRIVATE Vita3K.icns)
	set(MACOSX_BUNDLE_ICON_FILE Vita3K.icns)
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.Vita3K.Vita3K")
	set(MACOSX_BUNDLE_BUNDLE_NAME "Vita3K")

	message(STATUS "MoltenVK dylib path: ${MOLTENVK_DYLIB}")
	target_sources(vita3k PRIVATE ${MOLTENVK_DYLIB})
	set_source_files_properties(${MOLTENVK_DYLIB} PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks)

	qt_finalize_target(vita3k)
	if(MACDEPLOYQT_EXECUTABLE)
		add_custom_command(TARGET vita3k POST_BUILD
			COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE_DIR:vita3k>/../.." "$<$<CONFIG:Debug,RelWithDebInfo>:-no-strip>")
	endif()

	add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND codesign --force --deep --preserve-metadata=entitlements,requirements,flags,runtime --sign - "$<TARGET_FILE_DIR:vita3k>/../..")
elseif(LINUX)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")
	if(BUILD_APPIMAGE)
		set(APPDIR "${CMAKE_BINARY_DIR}/AppImage")
		add_custom_command(
			TARGET vita3k
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory "${APPDIR}/usr/share/Vita3K"
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${APPDIR}/usr/share/Vita3K/data"
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${VITA3K_QT_QM_DIR}" "${APPDIR}/usr/share/Vita3K/translations"
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "${APPDIR}/usr/share/Vita3K/shaders-builtin"
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/icons" "${APPDIR}/usr/share/Vita3K/icons"
        )
		set(LINUXDEPLOY_WRAPPER "${CMAKE_SOURCE_DIR}/appimage/build.sh")
		if(USE_DISCORD_RICH_PRESENCE)
			set(DISCORD_LIB_APPIMAGE "-l \"${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/libdiscord_game_sdk.so\"")
		else()
			set(DISCORD_LIB_APPIMAGE "")
		endif()
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${LINUXDEPLOY_WRAPPER} ${LINUXDEPLOY_COMMAND} --appdir="${APPDIR}"
			-e "$<TARGET_FILE:vita3k>" --icon-filename="vita3k"
			-i "${CMAKE_CURRENT_SOURCE_DIR}/../data/image/icon.png" -d "${CMAKE_CURRENT_SOURCE_DIR}/../appimage/vita3k.desktop"
			--custom-apprun="${CMAKE_CURRENT_SOURCE_DIR}/../appimage/apprun.sh" --plugin qt --output=appimage ${DISCORD_LIB_APPIMAGE}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/*.AppImage*" "$<TARGET_FILE_DIR:vita3k>/"
		COMMAND ${CMAKE_COMMAND} -E rm "${CMAKE_CURRENT_BINARY_DIR}/*.AppImage*")
	endif()
	add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "$<TARGET_FILE_DIR:vita3k>/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${VITA3K_QT_QM_DIR}" "$<TARGET_FILE_DIR:vita3k>/translations"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "$<TARGET_FILE_DIR:vita3k>/shaders-builtin"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/icons" "$<TARGET_FILE_DIR:vita3k>/icons"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/COPYING.txt" "$<TARGET_FILE_DIR:vita3k>/COPYING.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/README.md" "$<TARGET_FILE_DIR:vita3k>/README.md"
	)
	if(USE_DISCORD_RICH_PRESENCE)
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/libdiscord_game_sdk.so" "$<TARGET_FILE_DIR:vita3k>")
	endif()
elseif(WIN32)
	target_link_libraries(vita3k PRIVATE Version)
	target_sources(vita3k PRIVATE resource.h Vita3K.ico Vita3K.rc Windows.manifest)
	set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT vita3k)
	set_target_properties(vita3k PROPERTIES WIN32_EXECUTABLE TRUE VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$(Configuration)")
	add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "$<TARGET_FILE_DIR:vita3k>/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${VITA3K_QT_QM_DIR}" "$<TARGET_FILE_DIR:vita3k>/translations"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "$<TARGET_FILE_DIR:vita3k>/shaders-builtin"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/icons" "$<TARGET_FILE_DIR:vita3k>/icons"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/COPYING.txt" "$<TARGET_FILE_DIR:vita3k>/COPYING.txt"
	)
	if(USE_DISCORD_RICH_PRESENCE)
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/external/discord_game_sdk/lib/x86_64/discord_game_sdk.dll" "$<TARGET_FILE_DIR:vita3k>")
	endif()
	if(EXISTS "${OPENSSL_ROOT_DIR}")
		add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENSSL_ROOT_DIR}/bin/libssl-3-x64.dll" "$<TARGET_FILE_DIR:vita3k>"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENSSL_ROOT_DIR}/bin/libcrypto-3-x64.dll" "$<TARGET_FILE_DIR:vita3k>")
	endif()
	if(WINDEPLOYQT_EXECUTABLE)
		add_custom_command(TARGET vita3k POST_BUILD
			COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime --no-opengl-sw --no-patchqt
				--no-system-d3d-compiler --no-system-dxc-compiler
				--plugindir "$<IF:$<CXX_COMPILER_ID:MSVC>,$<TARGET_FILE_DIR:vita3k>/plugins,$<TARGET_FILE_DIR:vita3k>/share/qt6/plugins>"
				--translationdir "$<IF:$<CXX_COMPILER_ID:MSVC>,$<TARGET_FILE_DIR:vita3k>/translations,$<TARGET_FILE_DIR:vita3k>/share/qt6/translations>"
				--verbose 0 "$<TARGET_FILE:vita3k>")
	endif()
elseif(ANDROID)
	add_custom_command(
		TARGET vita3k
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../data" "${CMAKE_CURRENT_SOURCE_DIR}/../android/app/assets/data"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders-builtin" "${CMAKE_CURRENT_SOURCE_DIR}/../android/app/assets/shaders-builtin"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/icons" "${CMAKE_CURRENT_SOURCE_DIR}/../android/app/assets/icons"
	)
endif()

if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	# Enable Tracy profiling on the main compilation target
	target_link_libraries(vita3k PRIVATE tracy)
endif()
