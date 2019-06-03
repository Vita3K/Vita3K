add_library(
	app
	STATIC
	include/app/app.h
	include/app/config.h
	include/app/screen_render.h
	src/app.cpp
	src/config.cpp
	src/init.cpp
	src/screen_render.cpp
	src/sfo.cpp
	src/sfo.h
)

target_include_directories(app PUBLIC include)
target_link_libraries(app PUBLIC glutil host util)
target_link_libraries(app PRIVATE ${Boost_LIBRARIES} audio glbinding-aux gui io kernel microprofile rtc sdl2 spdlog yaml-cpp)
