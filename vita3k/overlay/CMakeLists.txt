add_library(
	overlay
	STATIC
	include/overlay/types.h
	include/overlay/compiled_resource.h
	include/overlay/font.h
	include/overlay/element.h
	include/overlay/controls.h
	include/overlay/layouts.h
	include/overlay/animation.h
	include/overlay/overlay.h
	include/overlay/user_interface.h
	include/overlay/input.h
	include/overlay/display_manager.h
	include/overlay/shader_compile_notice.h
	include/overlay/perf_overlay.h
	include/overlay/trophy_notification.h
	include/overlay/common_dialog.h
	include/overlay/shader_precompile_progress.h
	include/overlay/pause_overlay.h
	src/compiled_resource.cpp
	src/font.cpp
	src/element.cpp
	src/controls.cpp
	src/layouts.cpp
	src/animation.cpp
	src/overlay.cpp
	src/input.cpp
	src/display_manager.cpp
	src/shader_compile_notice.cpp
	src/perf_overlay.cpp
	src/trophy_notification.cpp
	src/common_dialog.cpp
	src/shader_precompile_progress.cpp
	src/pause_overlay.cpp
)

target_include_directories(overlay PUBLIC include)
target_link_libraries(overlay PUBLIC util)
target_link_libraries(overlay PRIVATE stb fmt dialog ime lang config SDL3::SDL3)
