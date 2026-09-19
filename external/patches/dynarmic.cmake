# The pinned Dynarmic ARM64 callback-only paths omit precise memory-abort
# checks. Keep the fix reproducible without requiring a private submodule SHA.
find_program(PATCH_EXECUTABLE patch REQUIRED)
set(_dynarmic_patch "${CMAKE_CURRENT_LIST_DIR}/dynarmic-arm64-callback-abort.patch")
set(_dynarmic_source "${CMAKE_CURRENT_LIST_DIR}/../dynarmic")
set(_dynarmic_patched_source "${CMAKE_BINARY_DIR}/external/dynarmic-patched")

# Keep the submodule pristine. Recreate the build source on each configure so
# it always starts from the pinned checkout and receives exactly this patch.
file(REMOVE_RECURSE "${_dynarmic_patched_source}")
file(MAKE_DIRECTORY "${_dynarmic_patched_source}")
file(COPY "${_dynarmic_source}/" DESTINATION "${_dynarmic_patched_source}")
file(REMOVE_RECURSE "${_dynarmic_patched_source}/.git")

execute_process(COMMAND "${PATCH_EXECUTABLE}" --dry-run --batch --forward -p1 -i "${_dynarmic_patch}"
    WORKING_DIRECTORY "${_dynarmic_patched_source}" RESULT_VARIABLE _can_apply
    OUTPUT_QUIET ERROR_VARIABLE _patch_error)
if(NOT _can_apply EQUAL 0)
    message(FATAL_ERROR "Dynarmic callback-abort patch conflicts with this checkout: ${_patch_error}")
endif()
execute_process(COMMAND "${PATCH_EXECUTABLE}" --batch --forward -p1 -i "${_dynarmic_patch}"
    WORKING_DIRECTORY "${_dynarmic_patched_source}" RESULT_VARIABLE _applied
    ERROR_VARIABLE _patch_error)
if(NOT _applied EQUAL 0)
    message(FATAL_ERROR "Cannot apply Dynarmic callback-abort patch: ${_patch_error}")
endif()
