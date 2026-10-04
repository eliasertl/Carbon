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
- Public Sans and Phosphor are assets, not libraries, and have no switches.

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

Every example accepts `--screenshot <file.png>` (render one settled frame offscreen, save it and exit),
`--theme light|dark` and `--scale <factor>`:

```sh
Build/Examples/MinimalIntegration/MinimalIntegration --theme dark
Build/Examples/MinimalIntegration/MinimalIntegration --screenshot shot.png --scale 2
```

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

Examples and tests are off by default in this mode. Carbon does not set global compiler flags or output
directories, and its warning flags apply only to its own targets.

## Formatting

Run `Scripts/Format.ps1` (Windows) or `Scripts/Format.sh` (Linux) before committing. Pass `-Check` / `--check`
to verify without modifying files.
