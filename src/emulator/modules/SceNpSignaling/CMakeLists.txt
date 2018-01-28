add_library(SceNpSignaling STATIC include/SceNpSignaling/exports.h src/SceNpSignaling.cpp)
target_include_directories(SceNpSignaling PUBLIC include)
target_link_libraries(SceNpSignaling PRIVATE module)
