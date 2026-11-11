add_library(
	v3kn
	STATIC
	include/v3kn/account.h
	include/v3kn/activity.h
	include/v3kn/friend.h
	include/v3kn/messages.h
	include/v3kn/storage.h
	include/v3kn/state.h
	include/v3kn/types.h
	src/account.cpp
	src/activity.cpp
	src/friend.cpp
	src/messages.cpp
	src/storage.cpp
)

target_include_directories(v3kn PUBLIC include)
target_link_libraries(v3kn PUBLIC dialog emuenv io np util packages)
target_link_libraries(v3kn PRIVATE miniz nlohmann_json::nlohmann_json pugixml::pugixml sdl)
