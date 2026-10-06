# Script mode: fails when code outside a backend folder uses a graphics API.
#
#   cmake -DCHECK_DIRS=<dir>[;<dir>...] [-DEXCLUDE_DIR=<dir>] -P CheckBackendIsolation.cmake
#
# CHECK_DIRS   directories whose .h and .cpp files are checked, recursively
# EXCLUDE_DIR  a directory below them that is skipped: Framework/src/Carbon/Backends
#
# Everything above the renderer backends must stay the same whichever backend is used, and must build and run
# without a GPU. So no file there may include a graphics API's header or name its types and functions.

cmake_minimum_required(VERSION 3.25) # a script has no project to take its policies from

if(NOT CHECK_DIRS)
    message(FATAL_ERROR "CheckBackendIsolation.cmake needs CHECK_DIRS")
endif()

# Headers of graphics APIs and their loaders, and the identifiers they define. Kept narrow enough that ordinary
# words ("glyph", "global") do not match.
set(include_pattern "#[ \t]*include[ \t]*[<\"](webgpu/|dawn/|vulkan/|GL/|GLES[0-9]*/|EGL/|glad/|KHR/|d3d|dxgi|Metal/|nvrhi/)")
# CMake's regular expressions have no \b, so an identifier's start is "line start or not an identifier character".
set(symbol_pattern "(wgpu::|WGPU[A-Z][A-Za-z]+|ID3D1[12]|IDirect3D|IDXGI|(^|[^A-Za-z0-9_])(Vk[A-Z][A-Za-z]+|vk[A-Z][A-Za-z]+\\(|VK_[A-Z]|gl[A-Z][A-Za-z]+\\(|GL_[A-Z]|GLuint|GLenum))")

set(violations "")
set(checked_count 0)
foreach(check_dir IN LISTS CHECK_DIRS)
    file(GLOB_RECURSE checked_files "${check_dir}/*.h" "${check_dir}/*.cpp")
    foreach(checked_file IN LISTS checked_files)
        if(EXCLUDE_DIR)
            string(FIND "${checked_file}" "${EXCLUDE_DIR}/" excluded_at)
            if(excluded_at EQUAL 0)
                continue()
            endif()
        endif()
        math(EXPR checked_count "${checked_count} + 1")
        file(STRINGS "${checked_file}" lines)
        set(line_number 0)
        foreach(line IN LISTS lines)
            math(EXPR line_number "${line_number} + 1")
            if(line MATCHES "${include_pattern}" OR line MATCHES "${symbol_pattern}")
                string(STRIP "${line}" stripped_line)
                list(APPEND violations "  ${checked_file}:${line_number}: ${stripped_line}")
            endif()
        endforeach()
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n" violation_text)
    message(FATAL_ERROR "Graphics APIs may only be used in Framework/src/Carbon/Backends:\n${violation_text}")
endif()
message(STATUS "${checked_count} files outside the renderer backends use no graphics API")
