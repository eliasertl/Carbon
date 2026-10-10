# Building Carbon

## Requirements

- CMake 3.25 or newer
- A C++20 compiler with `std::format`: MSVC 2022, GCC 13+ or Clang 17+
- Nothing else for the OpenGL 3.3 and OpenGL ES 3.0 backends, which have no build dependency: a clone builds the
  library, the tests and the examples without installing anything
- Optional, for the WebGPU renderer backend: an installed [Dawn](https://dawn.googlesource.com/dawn) (see
  [Installing Dawn](#installing-dawn)). Without Dawn, Carbon builds without that backend; the other backends and
  the examples are not affected. See [Renderer backends](Backends.md)
- Optional, for the Vulkan renderer backend: the Vulkan headers and loader, and `glslc`. The
  [Vulkan SDK](https://vulkan.lunarg.com) has all of them (its installer sets `VULKAN_SDK`, which CMake finds); on
  Ubuntu, install `libvulkan-dev glslc`. The validation layers (`vulkan-validationlayers`, part of the SDK) make
  the Vulkan tests check every call
- For the Direct3D 11 and 9 backends (Windows): `fxc` from the Windows SDK, which Visual Studio installs
- The git submodules: `git submodule update --init --recursive`
- Linux, for the examples and the OpenGL backend's tests: the X11 and Wayland development packages GLFW needs (on Ubuntu:
  `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev libxkbcommon-dev wayland-protocols`)

## Quick start

```sh
git clone --recurse-submodules https://github.com/eliasertl/Carbon.git
cd Carbon
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release
cmake --build Build --config Release --parallel
ctest --test-dir Build -C Release --parallel 8
Build/Examples/OpenGL/Gallery
```

This needs no Dawn: Carbon builds every backend whose dependencies it finds (OpenGL and OpenGL ES always, Vulkan
and Direct3D when their tools are installed) and prints them (`Carbon: renderer backends: ...`). With a
multi-configuration generator (Visual Studio) the configuration is picked at build time, by `--config` and `-C`,
and the executables are in a configuration subfolder: `Build\Examples\OpenGL\Release\Gallery.exe`. For the
WebGPU backend, install Dawn and add `-DCMAKE_PREFIX_PATH=<dawn-install>` to the configure command.

## Installing Dawn

The WebGPU backend renders with Dawn. It is optional: without Dawn, Carbon and the examples build with the other
backends. Carbon never builds Dawn. To use WebGPU, build and install it once, following Dawn's
[CMake quickstart](https://github.com/google/dawn/blob/main/docs/quickstart-cmake.md):

```sh
git clone https://dawn.googlesource.com/dawn
cd dawn
git checkout 91158020c0b1cb0ddb4dc1c2c29e5a4669374f0b   # the commit Carbon is tested against
cmake -S . -B out/Release -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_ENABLE_INSTALL=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/Release
cmake --install out/Release --prefix install/Release
```

Dawn takes a while to build. These options leave out what Carbon does not need and are what Carbon's CI workflow
uses:
`-DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_TESTS=OFF -DDAWN_USE_GLFW=OFF -DDAWN_ENABLE_DESKTOP_GL=OFF
-DDAWN_ENABLE_OPENGLES=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_CMD_TOOLS=OFF`. On Linux, Dawn needs the X11
development packages (`libx11-dev libx11-xcb-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
libxext-dev`); add `-DDAWN_USE_WAYLAND=ON` if your application creates Wayland surfaces.

**Linux: build Dawn with Clang.** Dawn at the pinned commit does not compile with GCC 13 (errors about a missing
`operator==` in `dawn::native`). Build Dawn with Clang (`CC=clang CXX=clang++`); Carbon itself can then be built
with GCC or Clang, since both use libstdc++ and Dawn is a static library. If configuring Dawn fails with an
error about `dawncpp_module`, also pass `-DDAWN_SUPPORTS_CXX_MODULES=OFF`: Dawn detected C++ module support that
CMake cannot use with your compiler. This is what Carbon's CI workflow does.

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
| `CARBON_BACKEND_WEBGPU` | `ON` when Dawn is found | Compile the WebGPU renderer backend into `Carbon`. `ON` without Dawn stops the configuration with an explanation. Optional: the examples run on any backend |
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
| `STB` | `stb` | `Carbon` (its PNG decoder, compiled in privately), examples and tests | header search for `stb_image.h` |
| `DAWN` | `dawn::webgpu_dawn` | `Carbon` (WebGPU backend) | `Dawn` (never built; only searched while `CARBON_BACKEND_WEBGPU` is not `OFF`) |

Rules:

- If the target named by `_NAME` already exists, Carbon links it as-is and neither builds nor searches. This is
  how a parent project supplies its own copy of a dependency.
- With `_BUILD=OFF`, Carbon calls `find_package` and then links exactly `_NAME`. If your package exports a
  different target (for example `Freetype::Freetype`), set `_NAME` to it.
- Bundled FreeType is built without HarfBuzz, PNG, zlib, bzip2 and Brotli, and bundled HarfBuzz is built against
  that FreeType target, with its raster library (`harfbuzz-raster`, which paints COLR color glyphs) and without
  libpng. Dependencies are built statically with their tests, examples, docs and install rules disabled.
- `CARBON_DEPS_HARFBUZZ_RASTER_NAME` (default `harfbuzz-raster`) is the target of HarfBuzz's raster library. A
  HarfBuzz found on the system is used with `harfbuzz::harfbuzz-raster` when its package has that target;
  Meson-built packages (vcpkg's, Linux distributions') install the library without one, so Carbon then takes
  `harfbuzz-raster` from the folder of HarfBuzz's library (and the installed `CarbonConfig.cmake` does the same
  for the application). Without the
  library Carbon still builds; COLR color glyphs (Segoe UI Emoji, Noto Color Emoji's COLRv1 version) are then drawn
  in the text color, while CBDT and sbix ones keep their colors, since Carbon decodes their PNG images itself.
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
# then open http://localhost:8000/OpenGLES/Gallery.html or OpenGLES/Minimal.html
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
- The examples are built for the OpenGL ES backend only, into `Examples/OpenGLES`: `Minimal`, `Gallery`,
  `CustomComponent` and `Reflection`. CustomTitleBar is left out, since a page has no window to move.
- One more target exists only here: the [web app](#the-web-app), `Examples/Web/index.html`.

## The web app

The [live demo](https://eliasertl.github.io/Carbon/) is the target `WebApp`
([Examples/WebApp](../Examples/WebApp/Main.cpp)), built only with Emscripten. It starts on a start screen with two
choices: the **Gallery**, the same pages as the Gallery example with a back button, and the **Documentation**, a
reader for every Markdown file in `Docs/` with a sidebar of all documents. Everything is drawn by Carbon: the
reader lays out headings, paragraphs, lists, tables, code blocks, links and the images of `Docs/Images` itself.
Links to other documents and to headings open them in the reader; links to other files of the repository open
them on GitHub. The address follows the screen (`#gallery/buttons`, `#docs/Components/Button.md`), so the
browser's Back button works and every page can be linked to. The app starts in the appearance the browser
prefers, and each screen has a Dark switch.

Build and open it locally, in a build directory configured as in [Emscripten](#emscripten-web-browsers):

```sh
cmake --build Build/Web --target WebApp
python -m http.server --directory Build/Web/Examples/Web
# then open http://localhost:8000
```

The page needs a web server: opened as a file, the browser does not let it load `index.data`. The build writes
four files to `Examples/Web`: `index.html`, `index.js`, `index.wasm`, and `index.data`, which holds the `Docs`
folder as it was when the app was linked. Every file in `Docs/` is a dependency of the link, and the list is
gathered again on every build, so a changed or new document is in the next build without any code change; a new
top-level document appears under Guides, a new folder as a group of its own.

| CMake variable | Default | Meaning |
| --- | --- | --- |
| `CARBON_WEBAPP_REPOSITORY_URL` | `https://github.com/eliasertl/Carbon` | The repository the start screen and the links to source files lead to |
| `CARBON_WEBAPP_SOURCE_REF` | `main` | The branch, tag or commit whose files those links open |

The tests check the documentation the reader shows: `MarkdownTests` parses every document in `Docs/` and fails
when a link leads to a document, heading or file that does not exist, or an image is missing.

### On phones and tablets

The web app works with fingers ([Phones and tablets](Mobile.md)). On a phone in portrait (compact width) the
Gallery and the documentation are navigation stacks: the list of pages or documents, from which a page slides in.
The address shows which: `#docs` is the list, `#docs/Layout.md` a document, and the browser's Back button and the
app's back buttons move between them without filling the history. The page reports the safe area
(`env(safe-area-inset-*)`, with `viewport-fit=cover`), the text size (Dynamic Type on iOS, the browser's default
font size elsewhere) and the on-screen keyboard (the visual viewport) to Carbon; pictures of the documentation
and the Gallery's images can be pinched. All of this is in
[Examples/Common/WebHost.cpp](../Examples/Common/WebHost.cpp) and works for every web example.

To try it without a phone, open the local server in Chrome, open DevTools (F12), turn on the device toolbar
(Ctrl+Shift+M) and choose an iPhone or an iPad; the rotate button switches the orientation. Typing goes to the
focused field through a hidden input element, as the on-screen keyboard of a phone does; DevTools shows no
keyboard, the safe area of a notch, or Dynamic Type, so check those on a real device. To reach the local server
from a phone in the same network, serve on all interfaces (`python -m http.server --bind 0.0.0.0`) and open the
computer's address; Safari needs `https` only for features Carbon does not use.

### Deploying to GitHub Pages

[.github/workflows/Pages.yml](../.github/workflows/Pages.yml) builds the web app with Emscripten and publishes it
on GitHub Pages. It runs only when started by hand; nothing deploys automatically.

1. Once: in the repository's **Settings > Pages**, set **Source** to **GitHub Actions**.
2. Each deploy: **Actions > Pages > Run workflow**, on the branch to publish (normally `main`).

The workflow builds the commit it runs on, sets `CARBON_WEBAPP_REPOSITORY_URL` to the repository and
`CARBON_WEBAPP_SOURCE_REF` to that commit, and publishes the four files. The site is at
`https://<owner>.github.io/<repository>/`, for this repository https://eliasertl.github.io/Carbon/; the run's
summary links to it.

## Android

`Examples/Android` is the Gallery as an Android app: a
[GameActivity](https://developer.android.com/games/agdk/game-activity) whose native code runs the shared Gallery
pages on the [OpenGL ES backend](Backends.md#opengl-es). It is a Gradle project of its own, built from the command
line; Android Studio is not needed. Everything below installs into the user's profile, without administrator
rights, except the emulator's hypervisor.

### Tools

| Tool | Version | Where it comes from |
|------|---------|---------------------|
| JDK | 17 (Temurin 17.0.20) | [adoptium.net](https://adoptium.net/temurin/releases/?version=17), the Windows x64 `.zip` |
| Android command-line tools | 23.0 | [developer.android.com](https://developer.android.com/studio#command-tools), "Command line tools only" for Windows |
| Android SDK platform | 36 | `sdkmanager` |
| NDK | 27.3.13750724 | `sdkmanager` |
| CMake (the SDK's) | 3.31.6 | `sdkmanager` |
| Gradle | 8.14.6 | the wrapper in `Examples/Android` downloads it |
| Android Gradle plugin | 8.13.2 | Gradle downloads it |
| games-activity | 4.4.2 | Gradle downloads it (`androidx.games:games-activity`) |

Step by step, in PowerShell:

1. **JDK.** Unzip the Temurin 17 archive, for example to `%LOCALAPPDATA%\Programs\Temurin`, and point `JAVA_HOME`
   at it:
   `[Environment]::SetEnvironmentVariable('JAVA_HOME', "$env:LOCALAPPDATA\Programs\Temurin\jdk-17.0.20.1+1", 'User')`.
2. **SDK.** Create the SDK folder `%LOCALAPPDATA%\Android\Sdk` and unzip the command-line tools so that
   `cmdline-tools\latest\bin\sdkmanager.bat` exists in it (the archive's `cmdline-tools` folder becomes `latest`). Set
   `ANDROID_HOME` to the SDK folder the same way, and add `%ANDROID_HOME%\platform-tools` and
   `%ANDROID_HOME%\emulator` to the user's `Path` for `adb` and `emulator`. Open a new terminal afterwards: programs
   see the new variables only when they start.
3. **Packages.** Accept the licenses, then install the packages:

   ```powershell
   $sdkmanager = "$env:ANDROID_HOME\cmdline-tools\latest\bin\sdkmanager.bat"
   & $sdkmanager --licenses            # answer y to each
   & $sdkmanager "platform-tools" "platforms/android-36" "build-tools/36.0.0" "ndk/27.3.13750724" "cmake/3.31.6"
   ```

   The package names take `/` here where Google's documentation writes `;`: PowerShell ends a command at `;`, and
   version 23 of `sdkmanager` then installs nothing without saying so. Gradle installs the build tools the Android
   Gradle plugin wants (35.0.0) by itself on the first build.
4. **Emulator (optional).** For testing without a phone:

   ```powershell
   & $sdkmanager "emulator" "system-images/android-36/google_apis/x86_64"
   & "$env:ANDROID_HOME\cmdline-tools\latest\bin\avdmanager.bat" create avd -n CarbonPhone -d pixel_7 `
       -k "system-images;android-36;google_apis;x86_64"
   ```

   In `%USERPROFILE%\.android\avd\CarbonPhone.avd\config.ini`, set `hw.gpu.enabled = yes` (the emulator then uses
   the PC's GPU) and keep `hw.keyboard = no`: with a hardware keyboard the emulator shows no on-screen keyboard,
   which is what a phone has. The emulator needs the Windows Hypervisor Platform. If `emulator -accel-check` does
   not report it, turn it on as administrator and restart Windows:
   `Enable-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform`. This is the only step that needs
   administrator rights.

### Building and running

```powershell
cd Examples\Android
.\gradlew.bat assembleDebug          # or assembleRelease, signed with the debug key
adb install -r ..\..\Build\Android\Gradle\outputs\apk\debug\CarbonGallery-debug.apk
adb shell am start -n io.github.eliasertl.carbon.gallery/.MainActivity
```

The first build downloads Gradle, the plugin and games-activity, and builds Carbon for two ABIs: `arm64-v8a` for
phones and tablets and `x86_64` for the emulator. Everything it writes goes to `Build/Android` (Gradle to
`Build/Android/Gradle`, the NDK's CMake trees to `Build/Android/Native`), as with the other build trees. To run on a
phone, turn on USB debugging in its developer options and connect it; `adb devices` lists it. Start the emulator
with `emulator -avd CarbonPhone -no-snapshot -no-boot-anim -gpu host`. `adb logcat -s CarbonGallery` shows Carbon's
log, and `cmd /c "adb exec-out screencap -p > shot.png"` takes a screenshot (through `cmd`, since PowerShell's `>`
changes binary output).

How the project is set up:

- **CMake.** [Examples/Android/CMakeLists.txt](../Examples/Android/CMakeLists.txt) adds the repository as a
  subdirectory with the OpenGL ES backend only and without examples, tests or benchmarks, compiles the Gallery's
  pages, and links `libGallery.so` with the static GameActivity library (`game-activity::game-activity_static`, found
  through prefab), EGL and OpenGL ES 3. Carbon's own CMake needs nothing Android-specific.
- **Gradle.** [Examples/Android/build.gradle.kts](../Examples/Android/build.gradle.kts): compile and target SDK 36,
  minimum SDK 28 (Android 9, the first with display cutouts), the NDK and CMake versions above, prefab on. The
  sources stay in the folder instead of Gradle's `src/main` tree; the Gradle wrapper (`gradlew.bat`,
  `gradle/wrapper`) is committed, as is usual, and its folder keeps the lowercase name the wrapper scripts expect.
- **The activity.** [MainActivity.java](../Examples/Android/Java/MainActivity.java) draws edge to edge, into the
  display cutout, and takes Back while the app has somewhere to go back to. The manifest declares the configuration
  changes the app handles itself (rotation, size, density, font size, dark mode), so the activity is never
  recreated: Carbon lays out the new size in the next frame.

What the native host does, and how to do the same in an application, is in
[Phones and tablets](Mobile.md#on-android).

## Examples and tests

Every example accepts `--screenshot <file.png>` (render a settled frame offscreen, save it and exit),
`--theme light|dark`, `--scale <factor>` and `--size <width>x<height>`:

```sh
Build/Examples/OpenGL/Minimal --theme dark
Build/Examples/OpenGL/Minimal --screenshot shot.png --scale 2
```

For screenshots of states that need input, and of parts of a window, there are more options:

| Option | Meaning |
| --- | --- |
| `--page <name>` | The Gallery page to start on, by its key (`selection`, `menus`, `dates`, `reflection`, ...), or the section of the Reflection example (`general`, `appearance`, `audio`, `network`) |
| `--show <name>` | Something the Gallery opens at startup: `menu`, `popover`, `alert`, `sheet`, `notification`, `datepicker`, `pathmenu`, `toolbaroverflow` |
| `--pointer`, `--click`, `--right-click <x>x<y>` | Put the pointer there, and click, before the screenshot is taken |
| `--file-drag <x>x<y>`, `--file-drop <x>x<y>` | Files from the system (two sample paths) are dragged over this position, or dropped there |
| `--drag <x>x<y>` | Press the left button at the `--pointer` position and drag to this one, holding the button while the screenshot is taken |
| `--safe-area <l>,<t>,<r>,<b>` | Safe area insets in points, as a phone reports them (`0,59,0,34` for an iPhone 15 in portrait) |
| `--text-scale <factor>` | The system's text size (`IO::SetTextScale`) |
| `--keyboard <height>` | While a text control is edited, an on-screen keyboard of this height covers the bottom of the display; the example draws a stand-in for it |
| `--touch` | Touch mode, as on a phone or a tablet; the scripted pointer is a finger: `--click` taps, `--right-click` presses long, `--drag` drags a finger |
| `--compose <text>` | After the click, an input method composes this text in the focused text field: `|` separates its clauses, the first is the one being converted, and `\uXXXX` or `\UXXXXXXXX` stands for a character |
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
antialiased edges, which the script ignores (it compares pixels with Pillow, `pip install pillow`). While CI is
disabled (see [Continuous integration](#continuous-integration)), run the script locally after every change that
affects an image and commit the images it replaced. To add a screenshot, add a line to the list and reference the
image from the page.

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
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release -DCARBON_BUILD_EXAMPLES=OFF -DCARBON_BUILD_TESTS=OFF
cmake --build Build --config Release
cmake --install Build --config Release --prefix <carbon-install>
```

Add `-DCMAKE_PREFIX_PATH=<dawn-install>` to the first command to include the WebGPU backend.

```cmake
find_package(Carbon CONFIG REQUIRED)            # add COMPONENTS Extensions Reflection to require them
target_link_libraries(MyApp PRIVATE Carbon::Carbon Carbon::Extensions Carbon::Reflection)
carbon_copy_dawn_runtime(MyApp)                # Windows: copies d3dcompiler_47.dll next to the executable
```

Configure the application with `-DCMAKE_PREFIX_PATH=<carbon-install>`, or with both prefixes when the package
includes the WebGPU backend: `-DCMAKE_PREFIX_PATH="<carbon-install>;<dawn-install>"`.

What gets installed:

- `lib/`: the static libraries `Carbon`, `CarbonExtensions` and `CarbonReflection`. Carbon is static, so an application links
  FreeType and HarfBuzz as well: copies built from the submodules are installed next to Carbon and exported as
  `Carbon::freetype`, `Carbon::harfbuzz` and `Carbon::harfbuzz-raster` (linked automatically through
  `Carbon::Carbon`). Copies that
  were found with `find_package` (`CARBON_DEPS_<NAME>_BUILD=OFF`) are found again by `CarbonConfig.cmake`.
- `include/`: the public headers only. Internal headers are not installed. `Carbon/Reflection/Detail/` is
  installed because the reflection templates are compiled in the application; it is not meant for direct use.
- `lib/cmake/Carbon/`: the package files.
- `share/doc/Carbon/`: Carbon's license, the third-party notices and, in `Licenses/`, the license texts of the
  embedded fonts, FreeType, HarfBuzz and stb. Ship them with your application.

`Carbon_BACKENDS` lists the renderer backends the package was built with. Their graphics libraries are not
installed with Carbon; the application's build must be able to find them: the same Dawn install for WebGPU, the
Vulkan loader for Vulkan. With
MSVC, install each configuration to its own prefix, as for Dawn. The package is compatible within one major
version (1.x).

[Tests/Package](../Tests/Package) is a small project that uses an installed Carbon; CI's Linux Clang job builds
it against the installed package.

## Package managers: vcpkg and Conan

[Packaging/](../Packaging) has a vcpkg port and a Conan 2 recipe for the released version (1.0.0). Both download
the `v1.0.0` tag from GitHub, build it with Carbon's own CMake (without examples, tests and benchmarks) and install
the same package as `cmake --install`, so an application uses it exactly as described in
[Installing Carbon](#installing-carbon). FreeType, HarfBuzz and stb come from the package manager through the
[dependency switches](#dependency-switches), not from `ThirdParty/`. GitHub's archive of a tag has no submodules,
so both also download the embedded fonts and icons (Public Sans, JetBrains Mono, Phosphor) from the commits the
submodules pin, each checked against its hash. Neither is in the official registries yet; use them from this
repository as an overlay port and a local recipe.

The renderer backends are vcpkg features and Conan options. The default is OpenGL 3.3 and OpenGL ES 3.0, which need
nothing; Dawn is never required.

| Backend | vcpkg feature | Conan option | Needs |
| --- | --- | --- | --- |
| OpenGL 3.3 | `opengl` (default) | `with_opengl` (default `True`) | nothing |
| OpenGL ES 3.0 | `opengles` (default) | `with_opengles` (default `True`) | nothing |
| Vulkan | `vulkan` | `with_vulkan` | vcpkg: the `vulkan` and `shaderc` ports; Conan: `vulkan-loader`, `vulkan-headers`, `shaderc` |
| Direct3D 11 | `dx11` (Windows) | `with_dx11` (Windows) | `fxc` from the Windows SDK |
| Direct3D 9 | `dx9` (Windows) | `with_dx9` (Windows) | `fxc` from the Windows SDK |
| WebGPU | `webgpu` | not available | vcpkg: the `dawn` port. ConanCenter has no Dawn |

### vcpkg

Classic mode, from a vcpkg checkout:

```sh
vcpkg install carbon --overlay-ports=<carbon-repository>/Packaging/vcpkg
vcpkg install "carbon[vulkan,dx11]" --overlay-ports=<carbon-repository>/Packaging/vcpkg   # with more backends
```

In manifest mode, list `carbon` (or `{ "name": "carbon", "features": ["vulkan"] }`) in your `vcpkg.json` and give
the overlay in `vcpkg-configuration.json`:

```json
{ "overlay-ports": ["<carbon-repository>/Packaging/vcpkg"] }
```

Then configure your project with vcpkg's toolchain file and use the package:

```sh
cmake -S . -B Build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
```

```cmake
find_package(Carbon CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE Carbon::Carbon Carbon::Extensions Carbon::Reflection)
```

Carbon is always a static library (`vcpkg_check_linkage(ONLY_STATIC_LIBRARY)`); its dependencies follow the
triplet. The licenses of the embedded fonts are installed in `share/carbon/Licenses`; ship them with your
application. vcpkg's HarfBuzz has the raster library, so COLR emoji keep their colors.

### Conan

Create the package from the recipe once, then require it from your project. Carbon needs C++20, and the Vulkan
option builds shaderc, which needs C++17, so pass `compiler.cppstd=20` for all contexts (or set it in your profiles):

```sh
conan create <carbon-repository>/Packaging/conan -s:a compiler.cppstd=20 --build=missing
conan create <carbon-repository>/Packaging/conan -s:a compiler.cppstd=20 --build=missing -o "carbon/*:with_vulkan=True"
```

A `conanfile.txt` for an application:

```ini
[requires]
carbon/1.0.0

[generators]
CMakeDeps
CMakeToolchain

[layout]
cmake_layout
```

```sh
conan install . -s:a compiler.cppstd=20 --build=missing
cmake --preset conan-default          # Linux and single-configuration generators: conan-release
cmake --build --preset conan-release
```

The recipe uses Carbon's own `CarbonConfig.cmake` rather than generated CMake files, so `find_package(Carbon)`,
the components, `Carbon_BACKENDS` and `carbon_copy_dawn_runtime` work as with an installed Carbon; FreeType and
HarfBuzz are found again from the files Conan generates for them. ConanCenter's HarfBuzz (12.3.0) predates the
raster library, so with Conan COLR emoji (Segoe UI Emoji) are drawn in the text color; CBDT and sbix emoji keep
their colors.

### How the packages were checked

On Windows (MSVC, `x64-windows` and Conan's default profile with C++20), [Tests/Package](../Tests/Package) was
built against the package each one installed, in Release and Debug, and ran: built-in widgets, extension
components, a custom component and a custom renderer backend, compiled against nothing but the installed headers.
The Vulkan and Direct3D backends were checked the same way with both (Release); vcpkg's `webgpu` feature, which
builds Dawn, was not.

With Visual Studio 2026, some ConanCenter recipes (`vulkan-loader`, for the Vulkan option) build with CMake 3.31,
which has no generator for it. Run Conan from a developer prompt with
`-c tools.cmake.cmaketoolchain:generator=Ninja`.

## Continuous integration

> **CI is currently disabled** to keep GitHub Actions usage costs down. The workflow no longer runs on pushes and
> pull requests, only when started by hand (Actions > CI > Run workflow); the comment at the top of the workflow
> says how to turn the triggers back on. Until then, build and test locally before pushing (Debug and Release,
> `ctest`, `Scripts/Format.ps1 -Check` or `Scripts/Format.sh --check`), and render the documentation screenshots
> yourself with `python Scripts/Screenshots.py`. The last automatic run was on 2026-10-06.
>
> The [Pages workflow](#deploying-to-github-pages), which deploys the web app, is separate and has only ever been
> manual.

When it is enabled, [.github/workflows/CI.yml](../.github/workflows/CI.yml) runs on every push to `main` and on
every pull request:

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
  changed (see [Documentation screenshots](#documentation-screenshots)). A run started by hand does not.

## Formatting

Run `Scripts/Format.ps1` (Windows) or `Scripts/Format.sh` (Linux) before committing. Pass `-Check` / `--check`
to verify without modifying files.
