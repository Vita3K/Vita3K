add_library(
	cheat
	STATIC
	include/cheat/cheat.h
	include/cheat/functions.h
	include/cheat/state.h
	src/cheat.cpp
	src/parser.cpp
)

target_include_directories(cheat PUBLIC include)
target_link_libraries(cheat PUBLIC mem util)

if(NOT ANDROID)
	add_executable(
		cheat-tests
		tests/cheat_tests.cpp
	)

	target_link_libraries(cheat-tests PRIVATE cheat googletest mem util)
	add_test(NAME cheat COMMAND cheat-tests)
endif()
