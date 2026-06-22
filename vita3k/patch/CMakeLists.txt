add_library(
  patch
  STATIC
  include/patch/patch.h
  include/patch/instructions.h
  include/patch/util.h
  src/patch.cpp
  src/instructions.cpp
  src/util.cpp
)

target_include_directories(patch PUBLIC include)
target_link_libraries(patch PRIVATE util)
