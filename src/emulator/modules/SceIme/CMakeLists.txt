add_library(SceIme STATIC include/SceIme/exports.h src/SceIme.cpp)
target_include_directories(SceIme PUBLIC include)
target_link_libraries(SceIme PRIVATE module)
