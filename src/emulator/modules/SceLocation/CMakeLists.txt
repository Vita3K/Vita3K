add_library(SceLocation STATIC include/SceLocation/exports.h src/SceLibLocation.cpp)
target_include_directories(SceLocation PUBLIC include)
target_link_libraries(SceLocation PRIVATE module)
