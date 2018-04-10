add_library(
	disasm
	STATIC
	src/disasm.cpp
	include/disasm/functions.h
	include/disasm/state.h
)

target_include_directories(disasm PUBLIC include)
target_include_directories(disasm PRIVATE ${capstone_INCLUDE_DIRS})
target_link_libraries(disasm PRIVATE capstone-static util)
