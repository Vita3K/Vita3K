add_library(SceCodecEngine STATIC include/SceCodecEngine/exports.h src/SceCodecEngineUser.cpp)
target_include_directories(SceCodecEngine PUBLIC include)
target_link_libraries(SceCodecEngine PRIVATE module)
