add_library(SceAtrac STATIC include/SceAtrac/exports.h src/SceAtrac.cpp)
target_include_directories(SceAtrac PUBLIC include)
target_link_libraries(SceAtrac PRIVATE module)
