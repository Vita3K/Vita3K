add_library(pvf
    include/pvf/state.h
    include/pvf/types.h
    src/pvf.cpp
    src/lib.cpp
    src/font.cpp
)

target_include_directories(pvf PUBLIC include)
target_link_libraries(pvf PUBLIC freetype2 freetype mem io)
