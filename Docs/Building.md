# Building Carbon

## Requirements

- CMake 3.25 or newer
- A C++20 compiler with `std::format`: MSVC 2022, GCC 13+ or Clang 17+
- An installed [Dawn](https://dawn.googlesource.com/dawn) (see [Installing Dawn](#installing-dawn))
- The git submodules: `git submodule update --init --recursive`
- Linux, examples only: the X11 and Wayland development packages GLFW needs (on Ubuntu:
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

Carbon never builds Dawn. Build and install it once, following Dawn's
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
`Framework/src/Carbon/Renderer/`.

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
| `CARBON_BUILD_EXAMPLES` | `ON` when Carbon is the top-level project | Build the examples (needs GLFW and stb) |
| `CARBON_BUILD_TESTS` | `ON` when Carbon is the top-level project | Build the unit tests (needs GoogleTest) |
| `CARBON_INSTALL` | `ON` when Carbon is the top-level project | Generate install rules and the `CarbonConfig.cmake` package |
| `CARBON_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings in Carbon targets as errors; used by CI |
| `CARBON_FORCE_ASSERTS` | `OFF` | Keep `CB_ASSERT` checks active in optimized builds |

## Dependency switches

Every dependency `<NAME>` in `FREETYPE`, `HARFBUZZ`, `GOOGLETEST`, `GLFW`, `STB` has two variables:

| Variable | Meaning |
| --- | --- |
| `CARBON_DEPS_<NAME>_BUILD` | `ON` (default): build the submodule in `ThirdParty/`. `OFF`: use `find_package`. |
| `CARBON_DEPS_<NAME>_NAME` | The CMake target Carbon links. |

| Dependency | Default target | Used by | `find_package` when `_BUILD=OFF` |
| --- | --- | --- | --- |
| `FREETYPE` | `freetype` | `Carbon` | `Freetype` |
| `HARFBUZZ` | `harfbuzz` | `Carbon` | `harfbuzz` |
| `GOOGLETEST` | `GTest::gtest_main` | tests | `GTest` |
| `GLFW` | `glfw` | examples | `glfw3` |
| `STB` | `stb` | examples | header search for `stb_image_write.h` |
| `DAWN` | `dawn::webgpu_dawn` | `Carbon` | `Dawn` (always found, no `_BUILD` switch) |

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

Fonts, the shader and the icon constants are generated at build time into `<build>/Framework/Generated` by the
CMake scripts in `Framework/CMake/` (no Python or other tools needed). Nothing generated is committed, and the
`Carbon` library needs no asset files at run time.

## Examples and tests

Every example accepts `--screenshot <file.png>` (render a settled frame offscreen, save it and exit),
`--theme light|dark`, `--scale <factor>` and `--size <width>x<height>`:

```sh
Build/Examples/MinimalIntegration/MinimalIntegration --theme dark
Build/Examples/MinimalIntegration/MinimalIntegration --screenshot shot.png --scale 2
```

For screenshots of states that need input, and of parts of a window, there are more options:

| Option | Meaning |
| --- | --- |
| `--page <name>` | The Gallery page to start on, by its key (`selection`, `menus`, `dates`, ...) |
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

`ctest` runs headless. Most tests need no GPU at all. The renderer tests create a real device and compare
rendered pixels; on a machine without a WebGPU adapter they report as skipped.

On Windows, Dawn needs `d3dcompiler_47.dll` next to the executable (see
[Integration](Integration.md#windows-d3dcompiler_47dll)). The examples and tests copy it from the Windows SDK;
set `CARBON_D3DCOMPILER_DLL` to its path if CMake cannot find it.

## Using Carbon from a parent project

```cmake
add_subdirectory(External/Carbon)
target_link_libraries(MyApp PRIVATE Carbon::Carbon Carbon::Extensions)
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
find_package(Carbon CONFIG REQUIRED)            # add COMPONENTS Extensions to require the extension library
target_link_libraries(MyApp PRIVATE Carbon::Carbon Carbon::Extensions)
carbon_copy_dawn_runtime(MyApp)                # Windows: copies d3dcompiler_47.dll next to the executable
```

Configure the application with both prefixes: `-DCMAKE_PREFIX_PATH="<carbon-install>;<dawn-install>"`.

What gets installed:

- `lib/`: the static libraries `Carbon` and `CarbonExtensions`. Carbon is static, so an application links
  FreeType and HarfBuzz as well: copies built from the submodules are installed next to Carbon and exported as
  `Carbon::freetype` and `Carbon::harfbuzz` (linked automatically through `Carbon::Carbon`). Copies that
  were found with `find_package` (`CARBON_DEPS_<NAME>_BUILD=OFF`) are found again by `CarbonConfig.cmake`.
- `include/`: the public headers only. Internal headers are not installed.
- `lib/cmake/Carbon/`: the package files.
- `share/doc/Carbon/`: Carbon's license, the third-party notices and, in `Licenses/`, the license texts of the
  embedded fonts, FreeType and HarfBuzz. Ship them with your application.

Dawn is not installed with Carbon; the application's build must be able to find the same Dawn install. With
MSVC, install each configuration to its own prefix, as for Dawn. The package is compatible within one minor
version (0.1.x).

[Tests/Package](../Tests/Package) is a small project that uses an installed Carbon; CI builds it on every
platform.

## Continuous integration

[.github/workflows/CI.yml](../.github/workflows/CI.yml) runs on every push and pull request:

- `clang-format` checks the formatting with the pinned clang-format version.
- **Dawn** is built once per platform at the pinned commit and stored in the Actions cache, keyed by the
  commit. Changing `DAWN_COMMIT` in the workflow rebuilds it.
- **Windows MSVC (Release)**, **Linux GCC (Debug)** and **Linux Clang (Release)** configure with warnings as
  errors, build everything, run the tests, render screenshots of the examples (uploaded as artifacts), install
  Carbon and build `Tests/Package` against the installed package.
- After a push to `main`, the Windows job also renders the documentation screenshots and commits the ones that
  changed (see [Documentation screenshots](#documentation-screenshots)).

## Formatting

Run `Scripts/Format.ps1` (Windows) or `Scripts/Format.sh` (Linux) before committing. Pass `-Check` / `--check`
to verify without modifying files.
