add_library(
	util
	STATIC
	src/util.cpp
	src/instrset_detect.cpp
)

target_include_directories(util PUBLIC include)
target_link_libraries(util PUBLIC Boost::boost Boost::filesystem config fmt spdlog http mem)
