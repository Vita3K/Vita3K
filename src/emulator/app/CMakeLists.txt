add_library(
	app
	STATIC
	include/app/app_functions.h
	include/app/discord.h
	include/app/load_self.h
	include/app/screen_render.h
	src/app_init.cpp
	src/app_private.h
	src/app_sfo.cpp
	src/app.cpp
	src/discord.cpp
	src/load_self.cpp
	src/screen_render.cpp
)

target_include_directories(app PUBLIC include)
target_link_libraries(app PUBLIC host vita-headers)
target_link_libraries(app PRIVATE elfio vita-toolchain)
