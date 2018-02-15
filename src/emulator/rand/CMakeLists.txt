add_library(
	rand
	STATIC
	src/prng.cpp
	include/rand/prng.h
)

target_include_directories(rand PUBLIC include)
