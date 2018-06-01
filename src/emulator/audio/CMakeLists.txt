add_library(
audio
STATIC
include/audio/functions.h
include/audio/state.h
src/audio.cpp
)

target_include_directories(audio PUBLIC include)
target_link_libraries(audio PUBLIC sdl2 vita-headers)
target_link_libraries(audio PRIVATE microprofile util)
