# Building Carbon

## Requirements

- CMake 3.25 or newer
- A C++20 compiler with `std::format`: MSVC 2022, GCC 13+ or Clang 17+
- For the WebGPU renderer backend, which the examples use: an installed [Dawn](https://dawn.googlesource.com/dawn)
  (see [Installing Dawn](#installing-dawn)). Without Dawn, Carbon builds without that backend (headless library and
  tests only); see [Renderer backends](Backends.md)
- For the Vulkan renderer backend: the Vulkan headers and loader, and `glslc`. The
  [Vulkan SDK](https://vulkan.lunarg.com) has all of them (its installer sets `VULKAN_SDK`, which CMake finds); on
  Ubuntu, install `libvulkan-dev glslc`. The validation layers (`vulkan-validationlayers`, part of the SDK) make
  the Vulkan tests check every call
- The git submodules: `git submodule update --init --recursive`
- Linux, for the examples and the OpenGL backend's tests: the X11 and Wayland development packages GLFW needs (on Ubuntu:
  `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev libxkbcommon-dev wayland-protocols`)

## Quick start

```sh
git clone --recurse-submodules https://github.com/eliasertl/Carbon.git
cd Carbon
cmake -S . -B Build -DCMAKE_PREFIX_PATH=<dawn-install>
cmake --build Build
ctest --test-dir Build
```

With a multi-configuration generator (Visual Studio) pick the configuration at build time:
`cmake --build Build --config Release` and `ctest --test-dir Build -C Release`.

## Installing Dawn

The WebGPU backend renders with Dawn, and the main examples use it. Carbon never builds Dawn. Build and install it
once, following Dawn's
[CMake quickstart](https://github.com/google/dawn/blob/main/docs/quickstart-cmake.md):

```sh
git clone https://dawn.googlesource.com/dawn
cd dawn
git checkout 91158020c0b1cb0ddb4dc1c2c29e5a4669374f0b   # the commit Carbon is tested against
cmake -S . -B out/Release -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_ENABLE_INSTALL=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/Release
cmake --install out/Release --prefix install/Release
```

Dawn takes a while to build. These options leave out what Carbon does not need and are what Carbon's CI uses:
`-DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_TESTS=OFF -DDAWN_USE_GLFW=OFF -DDAWN_ENABLE_DESKTOP_GL=OFF
-DDAWN_ENABLE_OPENGLES=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_CMD_TOOLS=OFF`. On Linux, Dawn needs the X11
development packages (`libx11-dev libx11-xcb-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
libxext-dev`); add `-DDAWN_USE_WAYLAND=ON` if your application creates Wayland surfaces.

**Linux: build Dawn with Clang.** Dawn at the pinned commit does not compile with GCC 13 (errors about a missing
`operator==` in `dawn::native`). Build Dawn with Clang (`CC=clang CXX=clang++`); Carbon itself can then be built
with GCC or Clang, since both use libstdc++ and Dawn is a static library. If configuring Dawn fails with an
error about `dawncpp_module`, also pass `-DDAWN_SUPPORTS_CXX_MODULES=OFF`: Dawn detected C++ module support that
CMake cannot use with your compiler. This is what Carbon's CI does.

Then point Carbon at the install prefix with `-DCMAKE_PREFIX_PATH=<dawn>/install/Release`, or set `Dawn_DIR` to
`<prefix>/lib/cmake/Dawn`. Dawn's API changes often; other commits may need small adjustments in
`Framework/src/Carbon/Backends/WebGPU/`.

**Dawn version.** Carbon is developed and tested against Dawn commit
`91158020c0b1cb0ddb4dc1c2c29e5a4669374f0b` (2026-09-03).

**MSVC: match the configuration.** Dawn installs a static library. MSVC cannot link Debug and non-Debug static
libraries together (error `LNK2038`), so a Debug build of Carbon needs a Debug Dawn install, and
Release/RelWithDebInfo builds need a Release or RelWithDebInfo Dawn install. Install each Dawn configuration to
its own prefix and use one build directory per configuration:

```sh
cmake -S . -B Build/Debug   -G Ninja -DCMAKE_BUILD_TYPE=Debug   -DCMAKE_PREFIX_PATH=<dawn>/install/Debug
cmake -S . -B Build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<dawn>/install/Release
```

Carbon warns at configure time when it detects this mismatch.

**Parent projects that already provide Dawn.** If a target named `CARBON_DEPS_DAWN_NAME` (default
`dawn::webgpu_dawn`) exists before Carbon is added, Carbon uses it and skips `find_package`.

## CMake options

| Option | Default | Meaning |
| --- | --- | --- |
| `CARBON_BUILD_EXTENSIONS` | `ON` | Build the `CarbonExtensions` library |
| `CARBON_BUILD_REFLECTION` | `ON` | Build the `CarbonReflection` library (skipped with a message when `CARBON_BUILD_EXTENSIONS` is `OFF`) |
| `CARBON_BUILD_EXAMPLES` | `ON` when Carbon is the top-level project | Build the examples (needs GLFW and stb) |
| `CARBON_BUILD_TESTS` | `ON` when Carbon is the top-level project | Build the unit tests (needs GoogleTest) |
| `CARBON_BUILD_BENCHMARKS` | `ON` when Carbon is the top-level project | Build `CarbonBenchmarks` (needs Google Benchmark and `CARBON_BUILD_EXTENSIONS`; not built for the web). See [Optimizations](Optimizations.md) |
| `CARBON_INSTALL` | `ON` when Carbon is the top-level project | Generate install rules and the `CarbonConfig.cmake` package |
| `CARBON_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings in Carbon targets as errors; used by CI |
| `CARBON_FORCE_ASSERTS` | `OFF` | Keep `CB_ASSERT` checks active in optimized builds |
| `CARBON_BACKEND_WEBGPU` | `ON` when Dawn is found | Compile the WebGPU renderer backend into `Carbon`. `ON` without Dawn stops the configuration with an explanation. The main examples need it |
| `CARBON_BACKEND_OPENGL` | `ON` | Compile the OpenGL 3.3 renderer backend into `Carbon`. It has no build dependency; its tests create their context with GLFW, which is then built for the tests as well |
| `CARBON_BACKEND_OPENGLES` | `ON` | Compile the OpenGL ES 3.0 / WebGL 2 renderer backend into `Carbon`. It has no build dependency; it shares its renderer with the OpenGL backend |
| `CARBON_BACKEND_VULKAN` | `ON` when the Vulkan headers, loader and `glslc` are found | Compile the Vulkan renderer backend into `Carbon`; `ON` without them, or without `glslc`, stops the configuration with an explanation. `Vulkan_GLSLC_EXECUTABLE` points CMake at a `glslc` elsewhere |
| `CARBON_BACKEND_DX11` | `ON` on Windows when `fxc` is found | Compile the Direct3D 11 renderer backend into `Carbon`. `fxc`, the HLSL compiler of the Windows SDK, compiles its shaders; CMake looks in the newest Windows SDK, and `CARBON_FXC_EXECUTABLE` points it elsewhere. `ON` elsewhere than on Windows, or without `fxc`, stops the configuration with an explanation |
| `CARBON_BACKEND_DX9` | `ON` on Windows when `fxc` is found | Compile the Direct3D 9 renderer backend into `Carbon`, with the same requirements as `CARBON_BACKEND_DX11` |

The backend options are decided once, at the first configuration, and then cached: after installing a dependency
later, pass `-DCARBON_BACKEND_<NAME>=ON`. CMake prints the enabled backends (`Carbon: renderer backends: ...`), a
target that links `Carbon::Carbon` sees `CARBON_HAS_BACKEND_<NAME>` defined for each of them, and an installed
package lists them in `Carbon_BACKENDS`.

## Dependency switches

Every dependency `<NAME>` in `FREETYPE`, `HARFBUZZ`, `GOOGLETEST`, `GOOGLEBENCHMARK`, `GLFW`, `STB` has two
variables:

| Variable | Meaning |
| --- | --- |
| `CARBON_DEPS_<NAME>_BUILD` | `ON` (default): build the submodule in `ThirdParty/`. `OFF`: use `find_package`. |
| `CARBON_DEPS_<NAME>_NAME` | The CMake target Carbon links. |

| Dependency | Default target | Used by | `find_package` when `_BUILD=OFF` |
| --- | --- | --- | --- |
| `FREETYPE` | `freetype` | `Carbon` | `Freetype` |
| `HARFBUZZ` | `harfbuzz` | `Carbon` | `harfbuzz` |
| `GOOGLETEST` | `GTest::gtest_main` | tests | `GTest` |
| `GOOGLEBENCHMARK` | `benchmark::benchmark` | benchmarks | `benchmark` |
| `GLFW` | `glfw` | examples, and the OpenGL backend's tests and benchmarks | `glfw3` |
| `STB` | `stb` | examples and tests | header search for `stb_image_write.h` |
| `DAWN` | `dawn::webgpu_dawn` | `Carbon` (WebGPU backend) | `Dawn` (never built; only searched while `CARBON_BACKEND_WEBGPU` is not `OFF`) |

Rules:

- If the target named by `_NAME` already exists, Carbon links it as-is and neither builds nor searches. This is
  how a parent project supplies its own copy of a dependency.
- With `_BUILD=OFF`, Carbon calls `find_package` and then links exactly `_NAME`. If your package exports a
  different target (for example `Freetype::Freetype`), set `_NAME` to it.
- Bundled FreeType is built without HarfBuzz, PNG, zlib, bzip2 and Brotli, and bundled HarfBuzz is built against
  that FreeType target. Dependencies are built statically with their tests, examples, docs and install rules
  disabled.
- Public Sans, JetBrains Mono and Phosphor are assets, not libraries, and have no switches.

Example — use system FreeType and HarfBuzz:

```sh
cmake -S . -B Build -DCMAKE_PREFIX_PATH=<dawn-install> \
  -DCARBON_DEPS_FREETYPE_BUILD=OFF -DCARBON_DEPS_FREETYPE_NAME=Freetype::Freetype \
  -DCARBON_DEPS_HARFBUZZ_BUILD=OFF -DCARBON_DEPS_HARFBUZZ_NAME=harfbuzz::harfbuzz
```

## What the build generates

Fonts, shaders and the icon constants are generated at build time into `<build>/Framework/Generated` by the
CMake scripts in `Framework/CMake/` (no Python needed). The Vulkan backend's GLSL is compiled to SPIR-V there by
`glslc`. Nothing generated is committed, and the
`Carbon` library needs no asset files at run time.

## Emscripten (web browsers)

Carbon builds with [Emscripten](https://emscripten.org) and runs in a web browser through WebGL 2 and the
[OpenGL ES backend](Backends.md#opengl-es). Install the Emscripten SDK, then configure through `emcmake`:

```sh
git clone https://github.com/emscripten-core/emsdk.git && cd emsdk
./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh                                  # emsdk_env.bat or emsdk_env.ps1 on Windows

emcmake cmake -S . -B Build/Web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build Build/Web
ctest --test-dir Build/Web                             # the tests run in Node
python -m http.server --directory Build/Web/Examples
# then open http://localhost:8000/Gallery/OpenGLESGallery.html or Minimal/OpenGLESMinimal.html
```

What changes in a web build:

- **Backends.** OpenGL ES is on and is the one to use; desktop OpenGL defaults to off. WebGPU and Vulkan are not
  found and stay off.
- **GLFW.** The examples use Emscripten's GLFW 3.4 port (`--use-port=contrib.glfw3`, fetched on first use) instead of
  the submodule: the page's canvas is the window. The example's canvas fills the browser window and follows it when
  it is resized (`emscripten::glfw3::MakeCanvasResizable(window, "window")`), and its framebuffer follows the device
  pixel ratio, so Carbon renders at the display's content scale. The frame loop reads the framebuffer size every
  frame, which is all a resize needs.
- **Your application** links with `-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1`
  (Carbon resolves its WebGL functions by name) and, because fonts and text need more than Emscripten's defaults,
  `-sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1MB`. The browser drives the frame loop: hand your frame function to
  `emscripten_set_main_loop`, as
  [Examples/Minimal/OpenGLESMinimal.cpp](../Examples/Minimal/OpenGLESMinimal.cpp) and `Examples/Common/ExampleApp.cpp` do;
  `Examples/Common/Shell.html` is a minimal page whose canvas covers the whole viewport. A page has no window to
  wait for, so the frame function polls events and returns.
- **Tests** run in Node (`-sNODERAWFS`). Node has no canvas, so the GPU tests are not instantiated there; the rest
  of the suite runs as on the desktop, as four CTest entries (`CarbonTests.Part1` to `Part4`) instead of one per
  test, because every start of the executable compiles the module again.
- The examples are built for the OpenGL ES backend only: `OpenGLESMinimal`, `OpenGLESGallery`,
  `OpenGLESCustomComponent` and `Reflection`. CustomTitleBar is left out, since a page has no window to move.

## Examples and tests

Every example accepts `--screenshot <file.png>` (render a settled frame offscreen, save it and exit),
`--theme light|dark`, `--scale <factor>` and `--size <width>x<height>`:

```sh
Build/Examples/Minimal/WebGPUMinimal --theme dark
Build/Examples/Minimal/WebGPUMinimal --screenshot shot.png --scale 2
```

For screenshots of states that need input, and of parts of a window, there are more options:

| Option | Meaning |
| --- | --- |
| `--page <name>` | The Gallery page to start on, by its key (`selection`, `menus`, `dates`, `reflection`, ...), or the section of the Reflection example (`general`, `appearance`, `audio`, `network`) |
| `--show <name>` | Something the Gallery opens at startup: `menu`, `popover`, `alert`, `sheet`, `notification`, `datepicker`, `pathmenu`, `toolbaroverflow` |
| `--pointer`, `--click`, `--right-click <x>x<y>` | Put the pointer there, and click, before the screenshot is taken |
| `--crop <x>,<y>,<width>,<height>` | Save only this area of the window, in points |
| `--section <key>[,<key>...]` | Gallery: scroll to these sections and save only their boxes. A key is a section's title in lower case without spaces or punctuation. Pointer positions are then relative to the box |
| `--extend <left>,<top>,<right>,<bottom>` | Grow the saved area, for a menu or popover that reaches out of a section |

In screenshot mode the Gallery shows a fixed day (October 5, 2026) as today, so that screenshots are the same on
every day.

## Documentation screenshots

The images in `Docs/Images` are rendered by the examples, never edited by hand.
[Docs/Images/Screenshots.txt](Images/Screenshots.txt) lists each image with the example and the options that
produce it, and `Scripts/Screenshots.py` renders them all:

```sh
python Scripts/Screenshots.py                    # all images, from Build/Release (or Build)
python Scripts/Screenshots.py --only Toolbar     # the images whose path contains a word
python Scripts/Screenshots.py --check            # fail if an image is out of date, change nothing
```

An image is replaced only when it looks different: renderings on different GPUs differ by a level or two in
antialiased edges, which the script ignores (it compares pixels with Pillow, `pip install pillow`). After every
push to `main`, CI renders the screenshots on Windows and commits the images that changed, so the documentation
follows the code. To add a screenshot, add a line to the list and reference the image from the page.

`ctest` runs headless, and `ctest --parallel <n>` runs it several times faster: starting the test executable
costs more than most tests. Most tests need no GPU at all and are CTest entries of their own. The renderer tests
run once per compiled-in backend (`Backends/RendererTests.<Name>/WebGPU` in GoogleTest's terms) and are one CTest
entry per backend (`Backends.WebGPU`, `Backends.Vulkan`, ...): the tests of an entry share one device, which takes
longer to create than a test takes to run, and the entries take turns on the GPU. They create a device without a
window, render offscreen and compare pixels, and fail on any warning or error Carbon or the API's validation
reports. On a machine where a backend cannot create a device its entry reports as skipped. To run a single
renderer test, start the executable: `Build/Tests/CarbonTests --gtest_filter=Backends/RendererTests.*/DX11`.
`BackendCompareTests` renders a fixed scene in both themes at scale 1 and 2 with every backend and compares it
with WebGPU's rendering; when it fails, the actual, reference and difference images are in
`<build>/Tests/BackendCompare/`.

On Windows, Dawn needs `d3dcompiler_47.dll` next to the executable (see
[Renderer backends](Backends.md#webgpu)). The examples and tests copy it from the Windows SDK;
set `CARBON_D3DCOMPILER_DLL` to its path if CMake cannot find it.

## Benchmarks

`CarbonBenchmarks` measures steady-state frames of the headless core (frame time, heap allocations, draw data) and
the render function of each backend that is compiled in. Use a Release build:

```sh
cmake --build Build/Release --target CarbonBenchmarks
python Scripts/Benchmarks.py --out Scratch/Benchmarks.json     # every benchmark, median of 3 repetitions
```

[Optimizations](Optimizations.md) describes the scenarios, the options and the numbers, and
`Scripts/BuildMetrics.py`, which measures what building and testing cost.

## Using Carbon from a parent project

```cmake
add_subdirectory(External/Carbon)
target_link_libraries(MyApp PRIVATE Carbon::Carbon Carbon::Extensions Carbon::Reflection)
```

Examples, tests and install rules are off by default in this mode. Carbon does not set global compiler flags or
output directories, and its warning flags apply only to its own targets.

## Installing Carbon

Carbon can also be installed and then found with `find_package`:

```sh
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<dawn-install> -DCARBON_BUILD_EXAMPLES=OFF -DCARBON_BUILD_TESTS=OFF
cmake --build Build
cmake --install Build --prefix <carbon-install>
```

```cmake
find_package(Carbon CONFIG REQUIRED)            # add COMPONENTS Extensions Reflection to require them
target_link_libraries(MyApp PRIVATE Carbon::Carbon Carbon::Extensions Carbon::Reflection)
carbon_copy_dawn_runtime(MyApp)                # Windows: copies d3dcompiler_47.dll next to the executable
```

Configure the application with both prefixes: `-DCMAKE_PREFIX_PATH="<carbon-install>;<dawn-install>"`.

What gets installed:

- `lib/`: the static libraries `Carbon`, `CarbonExtensions` and `CarbonReflection`. Carbon is static, so an application links
  FreeType and HarfBuzz as well: copies built from the submodules are installed next to Carbon and exported as
  `Carbon::freetype` and `Carbon::harfbuzz` (linked automatically through `Carbon::Carbon`). Copies that
  were found with `find_package` (`CARBON_DEPS_<NAME>_BUILD=OFF`) are found again by `CarbonConfig.cmake`.
- `include/`: the public headers only. Internal headers are not installed. `Carbon/Reflection/Detail/` is
  installed because the reflection templates are compiled in the application; it is not meant for direct use.
- `lib/cmake/Carbon/`: the package files.
- `share/doc/Carbon/`: Carbon's license, the third-party notices and, in `Licenses/`, the license texts of the
  embedded fonts, FreeType and HarfBuzz. Ship them with your application.

`Carbon_BACKENDS` lists the renderer backends the package was built with. Their graphics libraries are not
installed with Carbon; the application's build must be able to find them: the same Dawn install for WebGPU, the
Vulkan loader for Vulkan. With
MSVC, install each configuration to its own prefix, as for Dawn. The package is compatible within one minor
version (0.1.x).

[Tests/Package](../Tests/Package) is a small project that uses an installed Carbon; CI builds it on every
platform.

## Continuous integration

[.github/workflows/CI.yml](../.github/workflows/CI.yml) runs on every push and pull request. (It is paused at the
moment to save Actions minutes and runs only when started by hand; the comment at the top of the workflow says how
to turn it back on. While it is paused, the documentation screenshots are not updated by CI either: run
`python Scripts/Screenshots.py` after a change that affects them.)

- `clang-format` checks the formatting with the pinned clang-format version.
- **Dawn** is built once per platform at the pinned commit and stored in the Actions cache, keyed by the
  commit. Changing `DAWN_COMMIT` in the workflow rebuilds it.
- **Windows MSVC (Release)**, **Linux GCC (Debug)** and **Linux Clang (Release)** configure with warnings as
  errors and every renderer backend of their platform required, build everything (the benchmarks too, which are
  not run), run the tests with `ctest --parallel` and render screenshots of the examples (uploaded as artifacts).
  The Linux Clang job also installs Carbon and builds `Tests/Package` against the installed package.
- The Linux jobs compile through `ccache`, whose cache is kept between runs, so third-party code and sources
  that did not change are not compiled again. The Emscripten SDK (with the libraries Emscripten builds on first
  use) and the files the Windows job needs of the Vulkan SDK are cached by their pinned versions
  (`EMSDK_VERSION`, `VULKAN_SDK_VERSION` in the workflow).
- The Linux jobs run the backend tests on software devices: WebGPU and Vulkan on Mesa's lavapipe (with the
  Vulkan validation layers), OpenGL on llvmpipe under Xvfb. The Windows job has the Vulkan SDK's headers, import
  library, `glslc` and loader to build the Vulkan backend; it has no Vulkan or OpenGL 3.3 driver, so those tests
  skip there.
- After a push to `main`, the Windows job also renders the documentation screenshots and commits the ones that
  changed (see [Documentation screenshots](#documentation-screenshots)).

## Formatting

Run `Scripts/Format.ps1` (Windows) or `Scripts/Format.sh` (Linux) before committing. Pass `-Check` / `--check`
to verify without modifying files.
