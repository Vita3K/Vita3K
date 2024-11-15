add_library(
  patch
  STATIC
  include/patch/patch.h
  src/patch.cpp
)

target_include_directories(patch PUBLIC include)
target_link_libraries(patch PRIVATE util)
