add_library(SceNpDrm STATIC include/SceNpDrm/exports.h src/SceNpDrm.cpp src/SceNpDrmPackage.cpp)
target_include_directories(SceNpDrm PUBLIC include)
target_link_libraries(SceNpDrm PRIVATE module)
