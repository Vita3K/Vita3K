add_library(ScePromoterUtil STATIC include/ScePromoterUtil/exports.h src/ScePromoterUtil.cpp)
target_include_directories(ScePromoterUtil PUBLIC include)
target_link_libraries(ScePromoterUtil PRIVATE module)
