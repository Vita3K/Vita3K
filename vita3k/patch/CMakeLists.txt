add_library(
  patch
  STATIC
  include/patch/patch.h
  include/patch/instructions.h
  src/patch.cpp
  src/instructions.cpp
)

target_include_directories(patch PUBLIC include)
target_link_libraries(patch PRIVATE util)
