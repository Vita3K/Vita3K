add_library(SceMusicExport STATIC include/SceMusicExport/exports.h src/SceMusicExport.cpp)
target_include_directories(SceMusicExport PUBLIC include)
target_link_libraries(SceMusicExport PRIVATE module)
