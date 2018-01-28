add_library(SceLibLocation STATIC include/SceLibLocation/exports.h src/SceLibLocation.cpp)
target_include_directories(SceLibLocation PUBLIC include)
target_link_libraries(SceLibLocation PRIVATE module)
