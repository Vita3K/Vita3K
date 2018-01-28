add_library(SceVoiceQoS STATIC include/SceVoiceQoS/exports.h src/SceVoiceQoS.cpp)
target_include_directories(SceVoiceQoS PUBLIC include)
target_link_libraries(SceVoiceQoS PRIVATE module)
