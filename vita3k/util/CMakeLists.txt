add_library(
    util
    STATIC
    include/util/align.h
    include/util/bytes.h
    include/util/exit_code.h
    include/util/exec.h
    include/util/find.h
    include/util/fs.h
    include/util/function_info.h
    include/util/semaphore.h
    include/util/lock_and_find.h
    include/util/log.h
    include/util/preprocessor.h
    include/util/resource.h
    include/util/string_utils.h
    include/util/system.h
    include/util/types.h
    include/util/vector_utils.h
    src/util.cpp
)

target_include_directories(util PUBLIC include)
target_link_libraries(util PUBLIC ${Boost_LIBRARIES} spdlog)
