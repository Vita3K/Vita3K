add_library(
	bgm_player
	STATIC
	include/bgm_player/functions.h
	src/bgm_player.cpp
)

target_include_directories(bgm_player PUBLIC include)
target_link_libraries(bgm_player PUBLIC audio cubeb codec gui emuenv io)
