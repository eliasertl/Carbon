# Third-party dependencies.
#
# For every dependency <NAME> in {FREETYPE, HARFBUZZ, GOOGLETEST, GLFW, STB}:
#   CARBON_DEPS_<NAME>_BUILD  ON: build the git submodule in this folder. OFF: use find_package.
#   CARBON_DEPS_<NAME>_NAME   The CMake target Carbon links. If that target already exists (for
#                             example because a parent project provides it), nothing is built or searched.
# Dawn is never built here; see the end of this file. Docs/Building.md documents all of this.

# Imported targets created by find_package below must be visible in the sibling directories.
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)

# Scoped to this directory: build dependencies statically, without install rules, and let the
# cache-less variables below win over the dependencies' own option() defaults.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF)
set(SKIP_INSTALL_ALL ON)

# Declares the BUILD/NAME switches of one dependency.
function(carbon_declare_dependency NAME DEFAULT_TARGET)
    option(CARBON_DEPS_${NAME}_BUILD "Build ${NAME} from its ThirdParty submodule (OFF: use find_package)" ON)
    set(CARBON_DEPS_${NAME}_NAME "${DEFAULT_TARGET}" CACHE STRING "CMake target Carbon links for ${NAME}")
endfunction()

# Fails with a helpful message when a submodule has not been checked out.
function(carbon_require_submodule DIRECTORY MARKER)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${DIRECTORY}/${MARKER}")
        message(FATAL_ERROR
            "Carbon: the submodule ThirdParty/${DIRECTORY} is missing.\n"
            "Run: git submodule update --init --recursive")
    endif()
endfunction()

# Fails when the target a dependency should resolve to does not exist after build/find.
function(carbon_require_target NAME)
    if(NOT TARGET ${CARBON_DEPS_${NAME}_NAME})
        message(FATAL_ERROR
            "Carbon: the target '${CARBON_DEPS_${NAME}_NAME}' for ${NAME} does not exist.\n"
            "Set CARBON_DEPS_${NAME}_NAME to the target your package provides, or set "
            "CARBON_DEPS_${NAME}_BUILD=ON to build the bundled submodule. See Docs/Building.md.")
    endif()
endfunction()

# ---- FreeType ---------------------------------------------------------------------------------
# Built without its optional dependencies, in particular without HarfBuzz, to avoid a cycle.
carbon_declare_dependency(FREETYPE freetype)
if(NOT TARGET ${CARBON_DEPS_FREETYPE_NAME})
    if(CARBON_DEPS_FREETYPE_BUILD)
        carbon_require_submodule(FreeType CMakeLists.txt)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
        add_subdirectory(FreeType EXCLUDE_FROM_ALL)
    else()
        find_package(Freetype REQUIRED)
        if(NOT TARGET ${CARBON_DEPS_FREETYPE_NAME} AND TARGET Freetype::Freetype)
            add_library(${CARBON_DEPS_FREETYPE_NAME} INTERFACE IMPORTED GLOBAL)
            target_link_libraries(${CARBON_DEPS_FREETYPE_NAME} INTERFACE Freetype::Freetype)
        endif()
    endif()
endif()
carbon_require_target(FREETYPE)

# ---- HarfBuzz ---------------------------------------------------------------------------------
# Built against the FreeType target above; HarfBuzz enables its FreeType helpers when a target
# named `freetype` exists.
carbon_declare_dependency(HARFBUZZ harfbuzz)
if(NOT TARGET ${CARBON_DEPS_HARFBUZZ_NAME})
    if(CARBON_DEPS_HARFBUZZ_BUILD)
        carbon_require_submodule(HarfBuzz CMakeLists.txt)
        if(NOT TARGET freetype)
            add_library(freetype INTERFACE IMPORTED)
            target_link_libraries(freetype INTERFACE ${CARBON_DEPS_FREETYPE_NAME})
        endif()
        set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
        set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_RASTER OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_VECTOR OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_GPU OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_GPU_DEMO OFF CACHE STRING "" FORCE)
        add_subdirectory(HarfBuzz EXCLUDE_FROM_ALL)
    else()
        find_package(harfbuzz REQUIRED)
        if(NOT TARGET ${CARBON_DEPS_HARFBUZZ_NAME} AND TARGET harfbuzz::harfbuzz)
            add_library(${CARBON_DEPS_HARFBUZZ_NAME} INTERFACE IMPORTED GLOBAL)
            target_link_libraries(${CARBON_DEPS_HARFBUZZ_NAME} INTERFACE harfbuzz::harfbuzz)
        endif()
    endif()
endif()
carbon_require_target(HARFBUZZ)

# ---- GoogleTest (tests only) ------------------------------------------------------------------
if(CARBON_BUILD_TESTS)
    carbon_declare_dependency(GOOGLETEST GTest::gtest_main)
    if(NOT TARGET ${CARBON_DEPS_GOOGLETEST_NAME})
        if(CARBON_DEPS_GOOGLETEST_BUILD)
            carbon_require_submodule(GoogleTest CMakeLists.txt)
            set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
            set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # match Carbon's /MD runtime on MSVC
            add_subdirectory(GoogleTest EXCLUDE_FROM_ALL)
        else()
            find_package(GTest REQUIRED)
        endif()
    endif()
    carbon_require_target(GOOGLETEST)
endif()

# ---- GLFW and stb (examples only) -------------------------------------------------------------
if(CARBON_BUILD_EXAMPLES)
    carbon_declare_dependency(GLFW glfw)
    if(NOT TARGET ${CARBON_DEPS_GLFW_NAME})
        if(CARBON_DEPS_GLFW_BUILD)
            carbon_require_submodule(GLFW CMakeLists.txt)
            set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
            add_subdirectory(GLFW EXCLUDE_FROM_ALL)
        else()
            find_package(glfw3 REQUIRED)
        endif()
    endif()
    carbon_require_target(GLFW)

    # stb is header-only, so "building" it means wrapping the submodule in an interface target.
    carbon_declare_dependency(STB stb)
    if(NOT TARGET ${CARBON_DEPS_STB_NAME})
        if(CARBON_DEPS_STB_BUILD)
            carbon_require_submodule(Stb stb_image_write.h)
            add_library(${CARBON_DEPS_STB_NAME} INTERFACE)
            target_include_directories(${CARBON_DEPS_STB_NAME} SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Stb")
        else()
            find_path(CARBON_STB_INCLUDE_DIR stb_image_write.h PATH_SUFFIXES stb REQUIRED)
            add_library(${CARBON_DEPS_STB_NAME} INTERFACE IMPORTED GLOBAL)
            target_include_directories(${CARBON_DEPS_STB_NAME} SYSTEM INTERFACE "${CARBON_STB_INCLUDE_DIR}")
        endif()
    endif()
    carbon_require_target(STB)
endif()

# ---- Dawn (always found, never built) ---------------------------------------------------------
set(CARBON_DEPS_DAWN_NAME "dawn::webgpu_dawn" CACHE STRING "CMake target Carbon links for Dawn (WebGPU)")
if(NOT TARGET ${CARBON_DEPS_DAWN_NAME})
    find_package(Dawn CONFIG QUIET)
    if(NOT TARGET ${CARBON_DEPS_DAWN_NAME})
        message(FATAL_ERROR
            "Carbon: could not find Dawn (target '${CARBON_DEPS_DAWN_NAME}').\n"
            "Carbon does not build Dawn. Build and install it once, then point CMake at the install prefix:\n"
            "  cmake -S . -B Build -DCMAKE_PREFIX_PATH=<dawn-install-prefix>\n"
            "or set Dawn_DIR to the folder that contains DawnConfig.cmake (<prefix>/lib/cmake/Dawn).\n"
            "A parent project may also define the target itself before adding Carbon.\n"
            "See Docs/Building.md, section \"Installing Dawn\".")
    endif()
endif()

# A static Dawn built for another MSVC runtime (Debug vs. Release) fails at link time with
# LNK2038. Detect the common mismatch early and explain it.
if(MSVC AND CMAKE_BUILD_TYPE)
    get_target_property(CARBON_DAWN_CONFIGURATIONS ${CARBON_DEPS_DAWN_NAME} IMPORTED_CONFIGURATIONS)
    if(CARBON_DAWN_CONFIGURATIONS)
        set(CARBON_DAWN_IS_DEBUG OFF)
        if("DEBUG" IN_LIST CARBON_DAWN_CONFIGURATIONS)
            set(CARBON_DAWN_IS_DEBUG ON)
        endif()
        if((CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CARBON_DAWN_IS_DEBUG) OR
           (NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND CARBON_DAWN_IS_DEBUG))
            message(WARNING
                "Carbon: CMAKE_BUILD_TYPE is '${CMAKE_BUILD_TYPE}' but the Dawn package provides "
                "'${CARBON_DAWN_CONFIGURATIONS}'. With MSVC, Debug and non-Debug static libraries cannot be "
                "mixed (LNK2038). Use a Dawn install built for a matching configuration. See Docs/Building.md.")
        endif()
    endif()
endif()
