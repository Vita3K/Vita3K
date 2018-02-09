add_library(SceNpParty STATIC include/SceNpParty/exports.h src/SceNpPartyGameUtil.cpp)
target_include_directories(SceNpParty PUBLIC include)
target_link_libraries(SceNpParty PRIVATE module)
