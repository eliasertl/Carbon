# Getting started

This guide takes you from a clone of the repository to your first interface. It assumes you have built a CMake
project before.

## 1. Build Carbon and run the examples

You need CMake 3.25 or newer, a C++20 compiler (MSVC 2022, GCC 13+ or Clang 17+) and, for the examples, an
installed [Dawn](https://dawn.googlesource.com/dawn), Google's implementation of WebGPU, which the examples render
with. Carbon does not build Dawn; [Building](Building.md#installing-dawn) explains how to build and install it once.

```sh
git clone --recurse-submodules https://github.com/eliasertl/Carbon.git
cd Carbon
cmake -S . -B Build -DCMAKE_PREFIX_PATH=<dawn-install>
cmake --build Build
ctest --test-dir Build
```

With a Visual Studio generator, add `--config Release` to the build command and `-C Release` to `ctest`, and
use the configuration your Dawn install was built for.

Then look around:

```sh
Build/Examples/Gallery/WebGPUGallery              # every component; try the Dark and Reduce Motion switches
Build/Examples/Gallery/VulkanGallery --theme dark --page charts   # the same on another backend
Build/Examples/WebGPUMinimal/WebGPUMinimal
Build/Examples/CustomComponent/WebGPUCustomComponent
Build/Examples/CustomTitleBar/WebGPUCustomTitleBar  # a window whose title bar is drawn by Carbon
Build/Examples/Reflection/Reflection               # a settings window generated from a struct
```

(With a Visual Studio generator the executables are in a configuration subfolder, for example
`Build/Examples/Gallery/Release/WebGPUGallery.exe`. Each example exists once per backend that was built,
named `<Backend><Example>`.)

## 2. Add Carbon to your project

As a subdirectory (a git submodule, for example):

```cmake
add_subdirectory(External/Carbon)
target_link_libraries(MyApp PRIVATE Carbon::Carbon Carbon::Extensions)
```

or as an installed package:

```cmake
find_package(Carbon CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE Carbon::Carbon Carbon::Extensions)
```

`Carbon::Carbon` is the core library. `Carbon::Extensions` adds the larger components (sidebar, menus, tables,
charts, ...) and is optional. Both are static libraries with the fonts, icons and shaders built in: there are no
asset files to ship. Details and all options are in [Building](Building.md).

## 3. Create a context

Carbon never opens a window or creates a device. Your application, the *host*, does that with whatever it
already uses (GLFW, SDL, a game engine). It creates a context and connects it to its graphics API with a
[renderer backend](Backends.md), here WebGPU:

```cpp
#include <Carbon/Backends/WebGPU/WebGPUBackend.h> // the WebGPU backend
#include <Carbon/Carbon.h>                       // core
#include <Carbon/Extensions/Extensions.h>        // core + extension components

Carbon::ContextDescription description;
Carbon::Context* context = Carbon::CreateContext(description);

Carbon::WebGPUInitInfo info;
info.Device = device;                            // your wgpu::Device
info.ColorFormat = Carbon::TextureFormat::BGRA8Unorm;   // format of the pass Carbon will draw into
Carbon::WebGPUInit(info);
```

## 4. Run frames

Every frame, tell Carbon the size of the area and how much time has passed, forward the input your window
received, build the interface, and let Carbon draw into your render pass:

```cpp
Carbon::IO& io = Carbon::GetIO();
io.SetDisplaySize(widthInPoints, heightInPoints);
io.SetContentScale(pixelsPerPoint);              // 1.0, 1.5, 2.0, ...
io.SetDeltaTime(secondsSinceLastFrame);
io.AddMousePosEvent(x, y);                       // ... and buttons, wheel, keys, text: see Integration

Carbon::NewFrame();
BuildInterface();                                // step 5
Carbon::EndFrame();

wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
Carbon::WebGPURender(pass);                      // after your own drawing, if any
pass.End();
```

[Integration](Integration.md) covers this side in full: input forwarding, DPI, clipboard and cursor callbacks,
logging. [Examples/WebGPUMinimal](../Examples/WebGPUMinimal/Main.cpp) is a complete host in
one file, with GLFW.

## 5. Build an interface

An immediate-mode interface is code that runs every frame. There are no widget objects to create, keep in sync
or destroy: you call a function for each control, and your own variables are the state.

```cpp
struct Settings
{
    bool DarkMode = false;
    float Volume = 0.5f;
    std::string Name;
};

void BuildInterface(Settings& settings)
{
    Carbon::BeginVStack({ .Spacing = 12.0f, .Padding = 20.0f, .Width = 320.0f });

    Carbon::Text("Settings", { .Style = Carbon::TextStyle::Title1 });

    if (Carbon::Toggle("Dark Mode", &settings.DarkMode, { .Width = Carbon::Size::Fill() }))
        Carbon::SetTheme(settings.DarkMode ? Carbon::Theme::Dark() : Carbon::Theme::Light());

    Carbon::Slider("Volume", &settings.Volume, 0.0f, 1.0f, { .Width = Carbon::Size::Fill() });
    Carbon::TextField("Name", &settings.Name, { .Placeholder = "Your name", .Width = Carbon::Size::Fill() });

    Carbon::BeginHStack({ .Spacing = 8.0f, .Width = Carbon::Size::Fill() });
    Carbon::Spacer();
    if (Carbon::Button("Cancel"))
        Close();
    if (Carbon::Button("Save", { .Role = Carbon::ButtonRole::Prominent, .IsDefault = true }))
        Save(settings);
    Carbon::EndHStack();

    Carbon::EndVStack();
}
```

What this shows:

- **Controls return what happened.** `Button` returns `true` on the frame it is clicked; `Toggle`, `Slider` and
  `TextField` return `true` on frames their value changed. The value itself lives in your struct.
- **Layout is stacks.** `BeginVStack` / `BeginHStack` place their items in a column or row; `Spacer()` takes the
  free space; `Size::Fill()` stretches an item. Nothing is positioned by hand. See [Layout](Layout.md).
- **Options are optional.** The last argument of every call is a struct used with designated initializers. Name
  only the fields you want to change, in the order they are declared. See [Styling](Styling.md) for how
  per-call options, the style stack and the theme combine.
- **Labels are identities.** `"Volume"` identifies the slider. Two controls with the same label in the same
  place need different IDs: `"Delete##row3"`, or `PushID(index)` around the items of a loop.
- **Motion comes for free.** The switch glides, the theme cross-fades, hover and focus states animate, and all
  of it calms down with `Carbon::SetReduceMotion(true)`. See [Animation](Animation.md).
- **The keyboard works.** Tab moves between the controls, Space and Enter activate them, Enter presses the
  default button. See [Keyboard navigation](KeyboardNavigation.md).

## 6. Grow from here

A typical application window is a sidebar next to a scrolling page:

```cpp
Carbon::BeginHStack({ .Spacing = 0.0f, .Alignment = Carbon::VerticalAlignment::Top,
                      .Width = Carbon::Size::Fill(), .Height = Carbon::Size::Fill() });

Carbon::BeginSidebar("sidebar");
if (Carbon::SidebarItem("General", page == 0, { .Icon = Carbon::Icons::Gear }))
    page = 0;
if (Carbon::SidebarItem("Sound", page == 1, { .Icon = Carbon::Icons::SpeakerHigh }))
    page = 1;
Carbon::EndSidebar();

Carbon::BeginScrollView("page", { .Spacing = 16.0f, .Padding = 24.0f });
BuildPage(page);
Carbon::EndScrollView();

Carbon::EndHStack();
```

[Examples/Gallery](../Examples/Gallery) is built exactly like this and shows every component.

| To learn about | Read |
| --- | --- |
| What components exist and how each one is used | [Components](Components/README.md) |
| Stacks, sizes, spacers and scroll views | [Layout](Layout.md) |
| Themes, colors, the style stack, typography and icons | [Styling](Styling.md) |
| Springs, timing curves and reduced motion | [Animation](Animation.md) |
| Keys, focus order and the focus ring | [Keyboard navigation](KeyboardNavigation.md) |
| Popovers, menus, alerts and sheets | [Overlays](Overlays.md) |
| Building a component of your own | [Custom components](CustomComponents.md) |
| The host side: input, render pass, DPI, fonts | [Integration](Integration.md) |
| Graphics APIs, choosing a backend, writing your own | [Renderer backends](Backends.md) |
| CMake options, dependencies, installing Dawn and Carbon | [Building](Building.md) |
| How Carbon works inside | [Architecture](Architecture.md) |

## Things that surprise people

- **One frame of latency for sizes.** Layout is a single pass: a stack learns the size of its content when it
  ends and uses it on the next frame. New containers are therefore hidden for one frame and then fade in. If
  your host renders only on demand, keep rendering while `Carbon::IsAnimating()` is `true`.
- **Begin needs End.** Every `Begin...` needs its `End...` in the same frame; Carbon reports a missing one
  through the log and the assert callback and recovers. `Begin` functions that return a `bool` (popovers,
  menus, sheets) need their `End` only when they returned `true`.
- **Points, not pixels.** All sizes and positions are in points; Carbon multiplies by the content scale when it
  draws, so the same code is sharp on any display.
- **Nothing is printed.** Carbon logs only through the `Log` callback you provide. Set it while you develop:
  misuse is reported there.
