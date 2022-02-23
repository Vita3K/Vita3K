add_library(
	app
	STATIC
	include/app/functions.h
	include/app/discord.h
	src/app_init.cpp
	src/app.cpp
	src/discord.cpp
)

target_include_directories(app PUBLIC include)
target_link_libraries(app PUBLIC host)
if(USE_DISCORD_RICH_PRESENCE)
  target_link_libraries(app PUBLIC discord-rpc)
endif()
target_link_libraries(app PRIVATE gui)
