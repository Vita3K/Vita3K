add_library(
	app
	STATIC
	include/app/app_config.h
	include/app/app_functions.h
	include/app/discord.h
	include/app/screen_render.h
	src/app_config.cpp
	src/app_init.cpp
	src/app_private.h
	src/app_sfo.cpp
	src/app.cpp
	src/discord.cpp
	src/screen_render.cpp
)

target_include_directories(app PUBLIC include)
target_link_libraries(app PUBLIC glutil host util)
target_link_libraries(app PRIVATE ${Boost_LIBRARIES} audio discord-rpc glbinding-aux gui io kernel microprofile rtc sdl2 spdlog yaml-cpp)
