# Script mode: fails when a source file includes a Carbon header that is not public.
#
#   cmake -DCHECK_DIR=<dir> -DALLOWED_FILE=<file> [-DCHECK_LIST_FILE=<file>] -P CheckPublicIncludes.cmake
#
# CHECK_DIR        directory whose .h and .cpp files are checked, recursively
# ALLOWED_FILE     text file with one public header per line, as it is written in an include ("Carbon/Core/ID.h")
# CHECK_LIST_FILE  optional: text file with the files to check, relative to CHECK_DIR, instead of all of them.
#                  Files that do not exist there (generated headers) are skipped.
#
# CarbonExtensions and custom components must be buildable from the installed headers alone. Includes below
# "Carbon/Extensions/" are the extension library's own files and always allowed.

if(NOT CHECK_DIR OR NOT ALLOWED_FILE)
    message(FATAL_ERROR "CheckPublicIncludes.cmake needs CHECK_DIR and ALLOWED_FILE")
endif()

file(STRINGS "${ALLOWED_FILE}" allowed_headers)
if(CHECK_LIST_FILE)
    file(STRINGS "${CHECK_LIST_FILE}" listed_files)
    set(checked_files "")
    foreach(listed_file IN LISTS listed_files)
        if(EXISTS "${CHECK_DIR}/${listed_file}")
            list(APPEND checked_files "${CHECK_DIR}/${listed_file}")
        endif()
    endforeach()
else()
    file(GLOB_RECURSE checked_files "${CHECK_DIR}/*.h" "${CHECK_DIR}/*.cpp")
endif()
if(NOT checked_files)
    message(FATAL_ERROR "No source files found in ${CHECK_DIR}")
endif()

set(violations "")
foreach(checked_file IN LISTS checked_files)
    file(STRINGS "${checked_file}" include_lines REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]")
    foreach(include_line IN LISTS include_lines)
        string(REGEX MATCH "[<\"]([^>\"]+)[>\"]" unused "${include_line}")
        set(included "${CMAKE_MATCH_1}")
        if(included MATCHES "^Carbon/Extensions/")
            continue()
        endif()
        # Anything that names Carbon, or reaches into an internal header by a relative path, must be public.
        if(included MATCHES "^Carbon/" OR included MATCHES "Internal")
            if(NOT included IN_LIST allowed_headers)
                file(RELATIVE_PATH relative_file "${CHECK_DIR}" "${checked_file}")
                list(APPEND violations "  ${relative_file} includes \"${included}\"")
            endif()
        endif()
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n" violation_text)
    message(FATAL_ERROR "Only public Carbon headers may be included in ${CHECK_DIR}:\n${violation_text}")
endif()

list(LENGTH checked_files checked_count)
message(STATUS "${checked_count} files in ${CHECK_DIR} include public Carbon headers only")
