add_library(
	ctrl
	INTERFACE
)

target_include_directories(ctrl INTERFACE include)
target_link_libraries(ctrl INTERFACE sdl2)
