# Carbon
*“Persistence refines the miserable piece of carbon in you into the purest form of diamond.”* ― Tobi Delly

Carbon is an immediate-mode C++20 UI framework for tools, editors and applications on Windows and Linux. It has
the productivity of an immediate-mode API and the look and feel of macOS: calm, minimal, precise, with smooth
spring-driven motion. Carbon renders through WebGPU ([Dawn](https://dawn.googlesource.com/dawn)) into a render
pass that your application owns; it never creates windows, devices or OS hooks.

> **Status: early development.** The repository scaffold builds; the framework is being implemented milestone by
> milestone. See [Docs/Architecture.md](Docs/Architecture.md) for the design and the roadmap.

```cpp
Carbon::NewFrame();

Carbon::BeginVStack({ .Spacing = 12.0f, .Padding = 20.0f });
    Carbon::Text("Settings", { .Style = Carbon::TextStyle::LargeTitle });
    Carbon::Toggle("Dark Mode", &darkMode);
    if (Carbon::Button("Save", { .Role = Carbon::ButtonRole::Prominent }))
        Save();
Carbon::EndVStack();

Carbon::EndFrame();
Carbon::Render(pass); // your wgpu::RenderPassEncoder
```

## Building

Requirements: CMake 3.25+, a C++20 compiler (MSVC 2022, GCC 13+ or Clang 17+) and an installed Dawn.

```sh
git clone --recurse-submodules https://github.com/eliasertl/Carbon.git
cd Carbon
cmake -S . -B Build -DCMAKE_PREFIX_PATH=<dawn-install>
cmake --build Build
ctest --test-dir Build
```

With Visual Studio generators, add `--config Release` (or the configuration your Dawn install was built for) to
the build command and `-C Release` to `ctest`. [Docs/Building.md](Docs/Building.md) explains how to install Dawn
and documents every CMake option.

Developed and tested against Dawn commit `91158020c0b1cb0ddb4dc1c2c29e5a4669374f0b`.

## Documentation

- [Architecture](Docs/Architecture.md) — modules, frame lifecycle, draw list, layout, animation, styling
- [Building](Docs/Building.md) — CMake options, dependency switches, installing Dawn

## License

Carbon is released under the [MIT License](LICENSE). Third-party components and the embedded fonts are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
