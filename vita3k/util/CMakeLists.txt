add_library(
	util
	STATIC
	src/util.cpp
	src/instrset_detect.cpp
)

target_include_directories(util PUBLIC include)
target_link_libraries(util PUBLIC ${Boost_LIBRARIES} config fmt spdlog http mem)
target_compile_definitions(util PRIVATE $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_ENABLE>)

