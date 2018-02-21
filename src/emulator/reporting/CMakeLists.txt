add_library(
	reporting
	STATIC
	include/reporting/functions.h
	src/reporting.cpp
	src/state.h
)

target_include_directories(reporting PUBLIC include)
