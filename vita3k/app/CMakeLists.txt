add_library(
	app
	STATIC
	include/app/functions.h
	include/app/discord.h
	include/app/screen_render.h
	src/app_init.cpp
	src/app.cpp
	src/discord.cpp
	src/screen_render.cpp
)

target_include_directories(app PUBLIC include)
target_link_libraries(app PUBLIC discord-rpc host vita-headers)
target_link_libraries(app PRIVATE display gui)
