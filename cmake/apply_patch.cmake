# Applies PATCH (-p1) to the current directory once: a no-op when it is
# already applied, an error when it applies neither way (the patched
# source moved on; refresh the patch).
#   cmake -DPATCH=file.patch -P apply_patch.cmake
find_program(PATCH_PROGRAM patch REQUIRED)
execute_process(COMMAND ${PATCH_PROGRAM} -p1 -R --dry-run --silent --input ${PATCH}
                RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
if(already EQUAL 0)
    return()
endif()
execute_process(COMMAND ${PATCH_PROGRAM} -p1 --forward --input ${PATCH}
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${PATCH} does not apply")
endif()
