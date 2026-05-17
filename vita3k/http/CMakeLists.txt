add_library(
    http
    STATIC
    include/http/state.h
    src/http.cpp
)

target_include_directories(http PUBLIC include)
target_link_libraries(http PUBLIC util)
target_link_libraries(http PRIVATE ssl)
if (WIN32)
    target_link_libraries(http PRIVATE winsock)
endif()
