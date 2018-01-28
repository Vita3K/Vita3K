add_library(SceCtrl STATIC include/SceCtrl/exports.h src/SceCtrl.cpp)
target_include_directories(SceCtrl PUBLIC include)
target_link_libraries(SceCtrl PRIVATE module)
