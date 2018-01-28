add_library(ScePhotoExport STATIC include/ScePhotoExport/exports.h src/ScePhotoExport.cpp)
target_include_directories(ScePhotoExport PUBLIC include)
target_link_libraries(ScePhotoExport PRIVATE module)
