add_library(
rtc
STATIC
include/rtc/rtc.h
src/rtc.cpp
)

target_include_directories(rtc PUBLIC include)
target_link_libraries(rtc PUBLIC util)
