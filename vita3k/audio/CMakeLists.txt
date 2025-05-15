add_library(
    audio
    STATIC
    src/audio.cpp
    src/impl/sdl_audio.cpp
    src/impl/cubeb_audio.cpp)

target_include_directories(audio PUBLIC include)
target_link_libraries(audio PUBLIC SDL3::SDL3)
target_link_libraries(audio PRIVATE util cubeb kernel)
