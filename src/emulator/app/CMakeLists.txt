add_library(
	app
	STATIC
	include/app/app_functions.h
	include/app/discord.h
	include/app/screen_render.h
	src/app_init.cpp
	src/app.cpp
	src/discord.cpp
	src/screen_render.cpp
)

target_include_directories(app PUBLIC include)
target_link_libraries(app PUBLIC host discord-rpc vita-headers)
target_link_libraries(app PRIVATE glbinding-aux)
