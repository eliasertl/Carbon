# Third-party dependencies.
#
# For every dependency <NAME> in {FREETYPE, HARFBUZZ, GOOGLETEST, GOOGLEBENCHMARK, GLFW, STB}:
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

# Adds code to the installed CarbonConfig.cmake: a dependency that was found here, not built, has to be found
# again by projects that use the installed package.
function(carbon_add_package_dependency CODE)
    set_property(GLOBAL APPEND_STRING PROPERTY CARBON_PACKAGE_DEPENDENCY_CODE "${CODE}\n")
endfunction()

# Fails with a helpful message when a submodule has not been checked out.
function(carbon_require_submodule DIRECTORY MARKER)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${DIRECTORY}/${MARKER}")
        message(FATAL_ERROR
            "Carbon: the submodule ThirdParty/${DIRECTORY} is missing.\n"
            "Run: git submodule update --init --recursive")
    endif()
endfunction()

# Declares CARBON_BACKEND_<NAME> from what was found: FOUND is the result of looking for the dependency.
function(carbon_declare_backend NAME FOUND DESCRIPTION)
    if(NOT DEFINED CARBON_BACKEND_${NAME})
        set(CARBON_BACKEND_${NAME} ${FOUND} CACHE BOOL "${DESCRIPTION}")
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
        carbon_add_package_dependency("find_dependency(Freetype)")
        if(NOT TARGET ${CARBON_DEPS_FREETYPE_NAME} AND TARGET Freetype::Freetype)
            add_library(${CARBON_DEPS_FREETYPE_NAME} INTERFACE IMPORTED GLOBAL)
            target_link_libraries(${CARBON_DEPS_FREETYPE_NAME} INTERFACE Freetype::Freetype)
            carbon_add_package_dependency(
                "if(NOT TARGET ${CARBON_DEPS_FREETYPE_NAME})\n    add_library(${CARBON_DEPS_FREETYPE_NAME} INTERFACE IMPORTED)\n    target_link_libraries(${CARBON_DEPS_FREETYPE_NAME} INTERFACE Freetype::Freetype)\nendif()")
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
        # HarfBuzz is some seventy translation units that each include most of the library, and is written to
        # be compiled in one piece (its harfbuzz.cc). In a few batches it costs a fraction and still uses
        # several cores.
        set_target_properties(harfbuzz PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 12)
    else()
        find_package(harfbuzz REQUIRED)
        carbon_add_package_dependency("find_dependency(harfbuzz)")
        if(NOT TARGET ${CARBON_DEPS_HARFBUZZ_NAME} AND TARGET harfbuzz::harfbuzz)
            add_library(${CARBON_DEPS_HARFBUZZ_NAME} INTERFACE IMPORTED GLOBAL)
            target_link_libraries(${CARBON_DEPS_HARFBUZZ_NAME} INTERFACE harfbuzz::harfbuzz)
            carbon_add_package_dependency(
                "if(NOT TARGET ${CARBON_DEPS_HARFBUZZ_NAME})\n    add_library(${CARBON_DEPS_HARFBUZZ_NAME} INTERFACE IMPORTED)\n    target_link_libraries(${CARBON_DEPS_HARFBUZZ_NAME} INTERFACE harfbuzz::harfbuzz)\nendif()")
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
            if(EMSCRIPTEN)
                set(gtest_disable_pthreads ON CACHE BOOL "" FORCE) # the tests run single-threaded in Node
            endif()
            add_subdirectory(GoogleTest EXCLUDE_FROM_ALL)
        else()
            find_package(GTest REQUIRED)
        endif()
    endif()
    carbon_require_target(GOOGLETEST)
endif()

# ---- Google Benchmark (benchmarks only) -------------------------------------------------------
# The benchmarks measure the desktop library; there is no benchmark executable in a browser.
if(CARBON_BUILD_BENCHMARKS AND NOT EMSCRIPTEN)
    carbon_declare_dependency(GOOGLEBENCHMARK benchmark::benchmark)
    if(NOT TARGET ${CARBON_DEPS_GOOGLEBENCHMARK_NAME})
        if(CARBON_DEPS_GOOGLEBENCHMARK_BUILD)
            carbon_require_submodule(GoogleBenchmark CMakeLists.txt)
            set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
            set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
            set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
            set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
            set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
            add_subdirectory(GoogleBenchmark EXCLUDE_FROM_ALL)
        else()
            find_package(benchmark REQUIRED)
        endif()
    endif()
    carbon_require_target(GOOGLEBENCHMARK)
endif()

# ---- OpenGL: nothing to find --------------------------------------------------------------------
# The OpenGL backend includes no GL header and links no GL library: the host loads OpenGL and hands Carbon its
# GetProcAddress function. It is declared here because its tests need GLFW for a context.
# In a browser (Emscripten) there is WebGL 2, which is OpenGL ES 3.0, and no desktop OpenGL.
if(EMSCRIPTEN)
    set(carbon_desktop_opengl OFF)
else()
    set(carbon_desktop_opengl ON)
endif()
carbon_declare_backend(OPENGL ${carbon_desktop_opengl} "Build the OpenGL 3.3 renderer backend (no dependency)")
carbon_declare_backend(OPENGLES ON "Build the OpenGL ES 3.0 / WebGL 2 renderer backend (no dependency)")

# ---- GLFW (examples, and the OpenGL backend's tests and benchmarks) ---------------------------
if(CARBON_BUILD_EXAMPLES OR
   ((CARBON_BUILD_TESTS OR CARBON_BUILD_BENCHMARKS) AND (CARBON_BACKEND_OPENGL OR CARBON_BACKEND_OPENGLES)))
    carbon_declare_dependency(GLFW glfw)
    if(NOT TARGET ${CARBON_DEPS_GLFW_NAME})
        if(EMSCRIPTEN)
            # In a browser, Emscripten's port of GLFW 3.4 (the canvas as the window, Hi-DPI aware) stands in for the
            # submodule. Emscripten fetches it on first use.
            add_library(${CARBON_DEPS_GLFW_NAME} INTERFACE)
            target_compile_options(${CARBON_DEPS_GLFW_NAME} INTERFACE --use-port=contrib.glfw3)
            target_link_options(${CARBON_DEPS_GLFW_NAME} INTERFACE --use-port=contrib.glfw3)
        elseif(CARBON_DEPS_GLFW_BUILD)
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
endif()

# ---- stb (examples write screenshots, tests write the images of failed comparisons) -----------
if(CARBON_BUILD_EXAMPLES OR CARBON_BUILD_TESTS)
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

# ---- Renderer backends ------------------------------------------------------------------------
# CARBON_BACKEND_<NAME> compiles a renderer backend into the Carbon library. Left unset, a backend is
# enabled when its dependency is found (and the result is cached; pass -DCARBON_BACKEND_<NAME>=ON to
# insist later). Set to ON, a missing dependency stops the configuration with an explanation.


# ---- WebGPU: Dawn (found, never built) --------------------------------------------------------
set(CARBON_DEPS_DAWN_NAME "dawn::webgpu_dawn" CACHE STRING "CMake target Carbon links for Dawn (WebGPU)")
if(NOT TARGET ${CARBON_DEPS_DAWN_NAME} AND (NOT DEFINED CARBON_BACKEND_WEBGPU OR CARBON_BACKEND_WEBGPU))
    # Dawn's package links Threads::Threads without looking for it.
    find_package(Threads QUIET)
    find_package(Dawn CONFIG QUIET)
endif()
set(carbon_dawn_found OFF)
if(TARGET ${CARBON_DEPS_DAWN_NAME})
    set(carbon_dawn_found ON)
endif()
carbon_declare_backend(WEBGPU ${carbon_dawn_found} "Build the WebGPU renderer backend (needs Dawn)")
if(CARBON_BACKEND_WEBGPU AND NOT carbon_dawn_found)
    message(FATAL_ERROR
        "Carbon: CARBON_BACKEND_WEBGPU is ON but Dawn was not found (target '${CARBON_DEPS_DAWN_NAME}').\n"
        "Carbon does not build Dawn. Build and install it once, then point CMake at the install prefix:\n"
        "  cmake -S . -B Build -DCMAKE_PREFIX_PATH=<dawn-install-prefix>\n"
        "or set Dawn_DIR to the folder that contains DawnConfig.cmake (<prefix>/lib/cmake/Dawn).\n"
        "A parent project may also define the target itself before adding Carbon.\n"
        "Without Dawn, configure with -DCARBON_BACKEND_WEBGPU=OFF.\n"
        "See Docs/Building.md, section \"Installing Dawn\".")
endif()

# ---- Vulkan: the loader and headers, and glslc to compile the shaders (Vulkan SDK) ------------
if(NOT DEFINED CARBON_BACKEND_VULKAN OR CARBON_BACKEND_VULKAN)
    find_package(Vulkan QUIET COMPONENTS glslc)
endif()
set(carbon_vulkan_found OFF)
if(TARGET Vulkan::Vulkan AND Vulkan_GLSLC_EXECUTABLE)
    set(carbon_vulkan_found ON)
endif()
carbon_declare_backend(VULKAN ${carbon_vulkan_found} "Build the Vulkan renderer backend (needs the Vulkan SDK)")
if(CARBON_BACKEND_VULKAN AND NOT carbon_vulkan_found)
    if(TARGET Vulkan::Vulkan)
        set(carbon_vulkan_missing "glslc, the shader compiler, was not found")
    else()
        set(carbon_vulkan_missing "the Vulkan headers and loader were not found")
    endif()
    message(FATAL_ERROR
        "Carbon: CARBON_BACKEND_VULKAN is ON but ${carbon_vulkan_missing}.\n"
        "Install the Vulkan SDK (https://vulkan.lunarg.com), which has both, and make sure VULKAN_SDK is set, or\n"
        "on Linux the distribution's packages (Ubuntu: libvulkan-dev glslc). Point CMake at glslc with\n"
        "-DVulkan_GLSLC_EXECUTABLE=<path> if it is installed elsewhere.\n"
        "Without Vulkan, configure with -DCARBON_BACKEND_VULKAN=OFF.")
endif()

# ---- Direct3D: Windows, and fxc to compile the HLSL (Windows SDK) ------------------------------
# The Direct3D backends call only COM methods, so Carbon links nothing for them; fxc compiles their shaders at
# build time.
set(carbon_fxc_found OFF)
if(WIN32)
    file(GLOB carbon_sdk_bins "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/10.*/x64")
    list(SORT carbon_sdk_bins COMPARE NATURAL ORDER DESCENDING)
    find_program(CARBON_FXC_EXECUTABLE fxc HINTS ${carbon_sdk_bins} DOC "fxc, the HLSL compiler of the Windows SDK")
    if(CARBON_FXC_EXECUTABLE)
        set(carbon_fxc_found ON)
    endif()
endif()
foreach(carbon_d3d_backend IN ITEMS DX11 DX9)
    carbon_declare_backend(${carbon_d3d_backend} ${carbon_fxc_found}
        "Build the ${carbon_d3d_backend} renderer backend (Windows; needs fxc from the Windows SDK)")
    if(CARBON_BACKEND_${carbon_d3d_backend} AND NOT carbon_fxc_found)
        if(NOT WIN32)
            message(FATAL_ERROR "Carbon: CARBON_BACKEND_${carbon_d3d_backend} is ON, but Direct3D exists only on Windows.")
        endif()
        message(FATAL_ERROR
            "Carbon: CARBON_BACKEND_${carbon_d3d_backend} is ON but fxc, the HLSL compiler of the Windows SDK, was not
"
            "found. Install the Windows SDK, configure from a Visual Studio developer prompt, or point CMake at it
"
            "with -DCARBON_FXC_EXECUTABLE=<path to fxc.exe>.")
    endif()
endforeach()

# A static Dawn built for another MSVC runtime (Debug vs. Release) fails at link time with
# LNK2038. Detect the common mismatch early and explain it.
if(CARBON_BACKEND_WEBGPU AND MSVC AND CMAKE_BUILD_TYPE)
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
