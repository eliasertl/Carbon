# Carbon Architecture

Status: **approved plan** (2026-10-03); implementation follows the milestones in section 15. This document is the
design contract for Carbon v1. Sections marked *Decision* record choices made where the brief left room.

Carbon is an immediate-mode C++20 UI framework that looks like macOS 11–26 (flat, no translucency) and renders
through a renderer backend (WebGPU on Dawn, or one the application writes) into a target owned by the host. It ships as three static libraries: `Carbon` (core),
`CarbonExtensions` (components built only on Carbon's public extension API) and `CarbonReflection` (interface
generated from an application's enums and structs, built on both).

## 1. Principles

1. **The host owns the platform.** Carbon never creates windows, surfaces, devices or OS hooks. The host forwards
   input and hands the renderer backend a render pass, command buffer or framebuffer to draw into.
2. **Immediate mode, small retained state.** The UI is rebuilt from code every frame. Carbon keeps only per-ID state:
   animations, focus, scroll offsets, measured sizes, text-edit state.
3. **Logical points everywhere.** All public coordinates and sizes are in points. Pixels exist only in the renderer
   and the glyph rasterizer.
4. **GPU-free above the backends.** Everything except `Backends/` compiles and runs without a GPU and includes no
   graphics API, so layout, widgets and draw-list output are unit-testable headless, and identical whichever
   backend draws them.
5. **Extensions prove the API.** `CarbonExtensions` may include only public Carbon headers. If an extension
   component needs something, it becomes public, documented API.

## 2. Modules

```
Framework/src/Carbon/
├── Carbon.h              umbrella header for applications
├── Extension.h           umbrella header for component authors (extension API)
├── Core/                 Platform, Log, Assert, Math (Vec2, Rect, Color, EdgeInsets), ContentScale, UTF8, Hash, ID,
│                         State (per-ID storage), Context
├── Input/                IO, Input (queries), Key, MouseButton, InputEvent, Cursor
├── Draw/                 DrawList, DrawTypes (vertex, primitive, command, draw data), Squircle (CPU shape function)
├── Text/                 FontLibrary, Font, TextShaper, GlyphAtlas, TextLayout, Icons (generated)
├── Layout/               Stack, Spacer, Grid, Size, ScrollView, layout cursor
├── Animation/            Spring, Easing, AnimationSpec, Animator (per-ID state)
├── Style/                Theme, StyleColor, StyleVar, style stacks, TextStyle ramp
├── Interaction/          hit testing, ButtonBehavior, DragBehavior, focus and keyboard navigation
├── Overlay/              floating surfaces above the interface: stacking, pointer capture, focus scopes
├── Widgets/              Text, Button, Toggle, Slider, TextField, Image, Separator, Tooltip, ControlSize,
│                         ControlFeedback (hover and pressed feedback shared by all controls)
├── Renderer/             backend-neutral rendering: the public RendererBackend interface, the dispatch from
│                         core to the installed backend, glyph-atlas and host-texture bookkeeping, TextureFormat
├── Backends/             the only folder that uses graphics APIs, one subfolder per backend with its public
│   ├── WebGPU/           header, implementation and shaders: WebGPUBackend.h, WebGPURendererInternal, Shaders/
│   ├── Vulkan/           VulkanBackend.h, VulkanRendererInternal, VulkanAllocatorInternal, Shaders/ (GLSL 450)
│   ├── OpenGL/           OpenGLBackend.h, OpenGLRendererInternal, OpenGLFunctionsInternal, Shaders/ (GLSL 330 and
│   │                     GLSL ES 300), shared with OpenGL ES
│   ├── OpenGLES/         OpenGLESBackend.h: the OpenGL ES 3.0 / WebGL 2 API over the OpenGL renderer
│   ├── DX11/             DX11Backend.h, DX11RendererInternal, Shaders/ (HLSL, shader model 4.0)
│   └── DX9/              DX9Backend.h, DX9RendererInternal, Shaders/ (HLSL, shader model 3.0)
└── Assets/               declarations of the embedded fonts and shaders (bytes generated into the build tree)
```

Dependency direction (each layer uses only the ones above it):

```
Core → Input → Draw → Text → Animation → Style → Layout → Interaction → Overlay → Widgets
                                                                                    ↑
Renderer hands DrawList output and GlyphAtlas pixels to the installed backend; nothing above depends on it, and
only Backends/ depends on a graphics API.
```

`Extensions/src/Carbon/Extensions/` holds one header and source per extension component (Sidebar, TabView,
SegmentedControl, Chart, Popover, Menu, MenuBar, Toolbar, PopUpButton, PullDownButton, ComboBox, TokenField,
RadioGroup, DatePicker, DatePickerCalendar, Stepper, ProgressIndicator, SearchField, List, Table, OutlineView,
ColumnView, PathControl, SplitView, Alert, Sheet, ColorWell, Notification) and, in `Internal/`, what several of them
share: `SelectionList` (Sidebar, List, Table, OutlineView, ColumnView), `ColumnLayout` (Table, OutlineView),
`MenuInternal` (Menu, MenuBar), `RowClipping` (the arithmetic of rows of one height, for lists that add only
their visible rows) and `BuildState` (a component's scratch state between its Begin and End calls).

`Reflection/src/Carbon/Reflection/` is the reflection library: `Enum.h` and `Struct.h` (queries), `Macros.h`
(`CB_REFLECT_ENUM`, `CB_REFLECT_STRUCT`), the option structs and the umbrella `Reflection.h`. `Detail/` holds what
the public templates are made of: `Signature.h` (every compiler-specific trick), `EnumModel.h`, `StructModel.h`,
`FieldTie.h` (structured bindings for up to 64 fields), `Description.h` (what the macros expand to) and
`DisplayName.h` (labels). `Reflection/src` may include public Carbon and CarbonExtensions headers only.

**Public vs. internal headers.** Public headers are listed explicitly in `Framework/CMakeLists.txt` (a
`FILE_SET HEADERS`); only they are installed. Internal headers end in `Internal.h` or live in a `Internal/`
subfolder and use `namespace Carbon::Internal`. Three checks keep the boundary honest:

- a CMake script (`Framework/CMake/CheckPublicIncludes.cmake`), run by CTest and CI as the
  `PublicApiBoundary.*` tests, fails if `Extensions/` or `Examples/CustomComponent` include a non-public
  header, if `Reflection/` includes anything but public Carbon and CarbonExtensions headers, or if a public
  header includes an internal one;
- CI builds `CarbonExtensions`, `CarbonReflection` and `Examples/CustomComponent` against the *installed*
  package, where internal headers do not exist;
- a CMake script (`Framework/CMake/CheckBackendIsolation.cmake`), run as the `BackendIsolation` test, fails if a
  file outside `Framework/src/Carbon/Backends/` (core, `Extensions/`, `Reflection/`) includes a graphics API's
  header or names its types or functions.

## 3. Public API sketch (application side)

```cpp
#include <Carbon/Carbon.h>

// ---- Setup ----
Carbon::ContextDescription description;
description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message) {};
description.Callbacks.GetClipboardText = /* std::string() */;
description.Callbacks.SetClipboardText = /* void(std::string_view) */;
description.Callbacks.SetCursor = /* void(Carbon::Cursor) */;
Carbon::Context* context = Carbon::CreateContext(description);  // becomes current if none is; headless
Carbon::WebGPUInit({ .Device = device,                          // a renderer backend (Docs/Backends.md)
                     .ColorFormat = Carbon::TextureFormat::BGRA8Unorm });   // must match the host's pass
Carbon::SetTheme(Carbon::Theme::Dark());

// ---- Every frame ----
Carbon::IO& io = Carbon::GetIO();
io.SetDisplaySize(widthInPoints, heightInPoints);
io.SetContentScale(contentScale);
io.SetDeltaTime(deltaSeconds);
io.AddMousePosEvent(x, y);
io.AddMouseButtonEvent(Carbon::MouseButton::Left, true);
io.AddMouseWheelEvent(0.0f, -1.0f);
io.AddKeyEvent(Carbon::Key::Tab, true);
io.AddInputCharactersUTF8(text);
io.AddFocusEvent(true);

Carbon::NewFrame();
Carbon::BeginVStack({ .Spacing = 12.0f, .Padding = 20.0f, .Alignment = Carbon::Alignment::Leading });
    Carbon::Text("Settings", { .Style = Carbon::TextStyle::LargeTitle });
    Carbon::Toggle("Dark Mode", &m_DarkMode);
    Carbon::BeginHStack({ .Spacing = 8.0f });
        Carbon::Spacer();
        if (Carbon::Button("Cancel"))
            Close();
        if (Carbon::Button("Save", { .Role = Carbon::ButtonRole::Prominent }))
            Save();
    Carbon::EndHStack();
Carbon::EndVStack();
Carbon::EndFrame();

Carbon::WebGPURender(pass);                                     // wgpu::RenderPassEncoder, host-owned
```

Other application-level entry points:

```cpp
void DestroyContext(Context* context);
void SetCurrentContext(Context* context);
Context* GetCurrentContext();

void SetTheme(const Theme& theme);              // animates from the current theme
void SetReduceMotion(bool enabled);
bool IsAnimating();                             // true while anything moves; lets event-driven hosts idle

Font* AddFontFromMemory(std::span<const uint8_t> data, const FontDescription& description = {});
Font* AddFontFromFile(const std::filesystem::path& path, const FontDescription& description = {});

void PushID(std::string_view id);  void PushID(int64_t id);  void PopID();
void PushStyleColor(StyleColor color, Color value);  void PopStyleColor(int count = 1);
void PushStyleVar(StyleVar var, float value);        void PopStyleVar(int count = 1);
void PushDisabled(bool disabled = true);             void PopDisabled();

// IO outputs the host reads after NewFrame
bool IO::WantsMouse() const;  bool IO::WantsKeyboard() const;  bool IO::WantsTextInput() const;
```

### Per-call option structs

Every widget takes an optional trailing `<Widget>Options` aggregate, used with designated initializers. Every style
field is a `std::optional` that falls back to the style stack, then the theme.

```cpp
struct ButtonOptions
{
    ButtonRole Role = ButtonRole::Default;       // Default (bordered), Prominent, Plain, Destructive
    ControlSize ControlSize = ControlSize::Regular;
    std::string_view Icon;                       // e.g. Carbon::Icons::Plus
    Size Width = Size::Fit();
    bool Disabled = false;
    bool IsDefault = false;                      // responds to Return when nothing else consumes it
    std::optional<float> CornerRadius;
    std::optional<float> CornerSmoothing;
    std::optional<Color> Tint;
};
bool Button(std::string_view label, const ButtonOptions& options = {});
```

*Decision — naming.* The brief's `{ .Style = Carbon::TextStyle::LargeTitle }` makes `TextStyle` the type-ramp
enum, so the per-call structs cannot be called `…Style`. They are named `<Widget>Options` throughout
(`TextOptions`, `ButtonOptions`, `VStackOptions`, …).

### Core widgets

```cpp
void Text(std::string_view text, const TextOptions& options = {});
bool Button(std::string_view label, const ButtonOptions& options = {});
bool Toggle(std::string_view label, bool* value, const ToggleOptions& options = {});   // .Kind = Switch | Checkbox
bool Slider(std::string_view label, float* value, float min, float max, const SliderOptions& options = {});
bool TextField(std::string_view label, std::string* text, const TextFieldOptions& options = {});
void Image(TextureID texture, Vec2 size, const ImageOptions& options = {});   // TextureID from a backend
void Icon(std::string_view icon, const IconOptions& options = {});
void Separator(const SeparatorOptions& options = {});
void Tooltip(std::string_view text);            // attaches to the previous item
```

Widgets that change a value return `true` on the frame the value changed. IDs work like Dear ImGui: the label is
hashed with the ID stack; `"Label##suffix"` disambiguates, `"###id"` fixes the ID independently of the label.

### Layout

```cpp
void BeginVStack(const VStackOptions& options = {});   void EndVStack();
void BeginHStack(const HStackOptions& options = {});   void EndHStack();
void Spacer(const SpacerOptions& options = {});        // flexible; .MinLength, .Weight
void BeginScrollView(std::string_view id, const ScrollViewOptions& options = {});  void EndScrollView();

struct Size { static Size Fit(); static Size Fixed(float points); static Size Fill(float weight = 1.0f); };

struct VStackOptions
{
    std::optional<float> Spacing;
    EdgeInsets Padding;                                   // implicit from float, {h, v} or {l, t, r, b}
    Carbon::Alignment Alignment = Alignment::Leading;     // cross axis: Leading | Center | Trailing
    VerticalAlignment Justify = VerticalAlignment::Top;   // main axis when there is free space and no Spacer
    Size Width = Size::Fit();
    Size Height = Size::Fit();
    std::optional<Color> Background;                      // optional squircle backdrop (grouped boxes)
    std::optional<float> CornerRadius;
    std::string_view ID;                                  // optional stable identity
};
// HStackOptions mirrors this with VerticalAlignment Alignment (Top | Center | Bottom) and Alignment Justify.

Vec2 GetCursorPos();  void SetCursorPos(Vec2 position);   // escape hatch for absolute placement
```

## 4. Frame lifecycle

```
host: io.Set*/Add*Event      queue input (events are timestamp-ordered, nothing is applied yet)
NewFrame()                   apply queued input → IO state (pressed/released edges, key repeat, characters)
                             advance animation clock, theme transition, resolve pending focus requests
                             reset draw lists, layout stack, style stacks
widgets                      allocate rects from layout, run behaviours, emit draw primitives
EndFrame()                   close root layout, store measured sizes, resolve Tab navigation,
                             assert balanced stacks (ID, style, layout, clip), garbage-collect stale per-ID state,
                             push cursor shape through the callback, finalize draw data (layers merged in order)
<Name>Render(target)         the backend gets expired textures, atlas changes and the draw data
                             (RenderDrawData), uploads its buffers and records the draws
```

- **Single current context** (`g_Context`), as in Dear ImGui. All free functions operate on it.
- **Input queue.** `Add*Event` appends to a queue; `NewFrame` applies it in order. An event that would hide an
  earlier change stays queued for the next frame: a second change of the same button or key, a mouse move after a
  button change (so presses keep their position), and editing keys versus typed characters (so `a`, Backspace,
  `b` never collapses). Clicks and keystrokes are therefore never lost or reordered at low frame rates. Held keys
  repeat on Carbon's clock (0.4 s delay, 50 ms interval); host-side repeat events are ignored.
- **Hit-test stability.** Hover uses the current frame's rects but overlay occlusion uses the previous frame's
  overlay rects, so a widget under a popover never reacts on the frame the popover is submitted after it.
- **No steady-state allocations.** Draw buffers, the event queue and per-frame scratch memory keep their capacity;
  shaped text runs live in an LRU cache; per-ID state lives in pooled hash maps.

## 5. Draw list and renderer

### Draw list (GPU-free)

```cpp
struct DrawVertex { Vec2 Position; Vec2 Local; Vec2 UV; uint32_t Color; uint32_t Primitive; };   // 32 bytes
struct DrawPrimitive { Vec2 HalfSize; float Radius; float Smoothing; float StrokeWidth; float Softness; DrawPrimitiveKind Kind; uint32_t Reserved; };
struct DrawCommand { Rect ClipRect; TextureID Texture; uint32_t IndexOffset; uint32_t IndexCount; };
struct DrawData { span Vertices; span Indices; span Primitives; span Commands; Vec2 DisplaySize; float ContentScale; };

DrawList& GetDrawList();                 // the current frame's draw list
const DrawData& GetDrawData();           // merged output, valid from EndFrame until the next NewFrame

class DrawList
{
public:
    void PushLayer(DrawLayer layer);     // Background, Content (default), Overlay, Tooltip
    void PopLayer();
    void PushClipRect(const Rect& rect, bool intersectWithCurrent = true);
    void PopClipRect();
    void PushOpacity(float opacity);     // multiplies the alpha of following shapes; nests
    void PopOpacity();

    void AddRect(const Rect& rect, Color color);
    void AddSquircle(const Rect& rect, Color color, float radius, float smoothing = DefaultSmoothing);
    void AddSquircleStroke(const Rect& rect, Color color, float radius, float width, float smoothing = DefaultSmoothing);
    void AddFocusRing(const Rect& rect, Color color, float radius, float width, float offset, float smoothing = DefaultSmoothing);
    void AddCircle(Vec2 center, float radius, Color color);
    void AddCircleStroke(Vec2 center, float radius, Color color, float width);
    void AddLine(Vec2 from, Vec2 to, Color color, float width, bool roundCaps = true);
    void AddShadow(const Rect& rect, Color color, float radius, float blur, Vec2 offset);
    void AddImage(TextureID texture, const Rect& rect, const Rect& uv, Color tint, float radius = 0.0f);
    void AddGlyph(const Rect& rect, const Rect& uv, Color color);    // used by the text layer (M2)
};
```

Every shape is one quad (4 vertices, 6 indices) one pixel larger than the shape; `Local` carries the position
relative to the shape centre and `Primitive` indexes a per-frame primitive array (runs of identical shapes share
one entry, and all glyphs share one). Lines are rotated pills, circles are squircles without smoothing. The draw
list is in points; clip rects are in points and converted to pixel scissor rects by the renderer. Shapes entirely
outside the clip rect, fully transparent or empty are dropped. Vertex colors are straight-alpha sRGB; the shader
premultiplies.

Consecutive quads that share a clip rect and texture merge into one `DrawCommand`. Untextured shapes never
sample, so they join whatever command is current; shapes and glyphs therefore share the atlas command and a
typical frame is a handful of draw calls. Only images with different textures break batches.

There is one draw list per context with four **layers** — `Background`, `Content`, `Overlay`, `Tooltip`.
Vertices and primitives are shared; each layer has its own index list, command list and clip stack (so an overlay
opened inside a clipped scroll view is not clipped by it). `EndFrame` concatenates the layers' indices back to
front into `DrawData`. That is the whole overlay mechanism on the drawing side.

### Squircle shape function

`Draw/Squircle.h` holds the pure-CPU function that tests use and the WGSL shader mirrors line for line:

```cpp
float SquircleDistance(Vec2 point, Vec2 halfSize, float radius, float smoothing);  // < 0 inside, in points
```

*Decision — shape model.* Apple's continuous corner is modelled as a superellipse corner patch:

- `radius` clamps to `min(halfSize.x, halfSize.y)`.
- The corner patch extends `p = radius * (1 + smoothing)` along each edge (clamped to the half side; when the
  clamp bites, the effective smoothing shrinks, so pills and circles degrade to exact circular arcs like Apple's
  capsules).
- Inside the patch the outline is `(u/p)^n + (v/p)^n = 1`, with the exponent `n` chosen so the curve passes
  through the same 45° apex as the circular arc of `radius`: `n = -ln 2 / ln(1 - (1 - 1/√2) / (1 + smoothing))`.
  Smoothing 0 gives `n = 2`, an exact circular rounded rectangle. The default 0.6 gives `n ≈ 3.4`; 1.0 gives
  `n ≈ 4.4`. For `n > 2` curvature is zero where the patch meets the straight edge, which is the "continuous
  curvature" property.
- Distance is the implicit value divided by its gradient length (first-order exact near the edge), which is what
  antialiasing, strokes and focus rings need. The gradient's length depends on direction and is undefined at the
  patch's inner corner, so the correction fades out with distance from the outline; the function is continuous
  everywhere, which soft shadows rely on.

Strokes are the difference of two squircles (outer shape minus the inset shape with `radius - width`), and focus
rings are a stroke on the outset shape, so ring corners stay concentric with the control.

### Renderer backend interface

`Carbon/Renderer/RendererBackend.h` is the only contract between Carbon's core and a graphics API. A context has
at most one backend; without one it is headless and still builds draw data.

```cpp
inline constexpr uint32_t RendererBackendVersion = 1;       // pinned by backends outside the repository

struct RendererBackendCapabilities { uint32_t MaxTextureSize = 4096; };
struct GlyphAtlasUpdate { span Pixels; uint32_t Width, Height, Generation, FirstRow, RowCount; bool IsFull; };

class RendererBackend
{
public:
    virtual ~RendererBackend();                                         // the shutdown
    virtual std::string_view GetName() const = 0;
    virtual RendererBackendCapabilities GetCapabilities() const;
    virtual void BeginFrame(uint64_t frameCount);                       // from NewFrame; no GPU work
    virtual void EndFrame();                                            // from EndFrame; no GPU work
    virtual void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) = 0;
    virtual void Render(const DrawData& drawData) = 0;
    virtual void ReleaseTexture(TextureID texture) = 0;
};

bool InstallRendererBackend(std::unique_ptr<T> backend);   T* GetRendererBackend<T>();
RendererBackend* GetRendererBackend();                     void RemoveRendererBackend();

// Called by a backend's own functions:
void RenderDrawData();        void FlushGlyphAtlas();      void InvalidateGlyphAtlas();
TextureID RegisterHostTexture(uint64_t key);               void ReleaseHostTexture(uint64_t key);
```

- **Native objects never cross the interface.** A backend has functions of its own that take the host's device,
  command buffer or render pass. Its render function stores the target and calls `RenderDrawData()`, which hands
  over expired host textures, glyph-atlas changes and then the draw data.
- **What is the same for every backend lives in core** (`Renderer/RendererBackend.cpp`, state in
  `RenderStateInternal.h`): which atlas rows a backend has not seen yet (all of them for a new generation, after
  install and after `InvalidateGlyphAtlas`), the registry of host textures, the check that rendering follows
  `EndFrame`, and the error for rendering without a backend.
- **Host textures.** `RegisterHostTexture(key)` marks a texture as used in the current frame; `EndFrame` marks
  every texture the frame's commands draw, registered or not, so a texture drawn by its raw handle
  (`MakeTextureID`) is tracked too and the backend resolves it lazily in `Render`. A texture last used in frame N expires at the start of frame N + 2.
  The backend hears about it (`ReleaseTexture`) with the next `RenderDrawData` or `FlushGlyphAtlas`, never from
  `NewFrame`: a backend may only touch the GPU inside calls its own functions make.
- **Frames in flight.** `EndFrame` tells the backend that new draw data is final; every `Render` until the next
  one receives the same data. A backend that must not overwrite buffers the GPU still reads advances its ring of
  buffers once per frame, however often the frame is drawn.
- **Limits.** `GetCapabilities().MaxTextureSize` caps the glyph atlas (at most 4096 either way).

### WebGPU backend (Dawn)

The first backend, in `Backends/WebGPU/`; `WebGPUInit`, `WebGPUShutdown`, `WebGPURender(pass)`,
`WebGPUGetTextureID(view)` and `WebGPUImage(view, ...)` in `WebGPUBackend.h`.

- One render pipeline, created for the host's `ColorFormat`, `DepthStencilFormat` and `SampleCount`. Premultiplied
  alpha blending, no depth write, depth test always.
- Bind group 0: frame uniforms (display size, content scale, sRGB flag) and the primitive storage buffer. Bind
  group 1: texture + sampler, switched per draw command (glyph atlas `R8Unorm`, or a host texture).
- The fragment shader switches on `Kind`: squircle fill, squircle stroke, line/capsule, shadow, glyph (atlas
  coverage), image (texture × tint, masked by a squircle so images can have smooth corners).
  Antialiasing is `clamp(0.5 - distance * scale, 0, 1)` in pixels, so edges are crisp at any content scale.
- Vertex, index and primitive buffers are written with `queue.WriteBuffer`, grow geometrically and are reused.
- Colors are authored in sRGB. For `…Unorm` targets Carbon writes them as-is (gamma-space blending, which is what
  macOS UI looks like); for `…UnormSrgb` targets the shader linearizes.
- WGSL lives in `Backends/WebGPU/Shaders/Carbon.wgsl` and is embedded at build time, only when the backend is
  built.
- Host textures: `WebGPUGetTextureID(view)` registers a `wgpu::TextureView` with its pointer as key and returns
  an opaque `TextureID`. The backend keeps the view and its bind group until core calls `ReleaseTexture`.
- GPU tests render offscreen and compare every pixel of a squircle with the CPU shape function, so the shader
  cannot drift from `Squircle.cpp` unnoticed. They skip on machines without an adapter.
- `WebGPURender(pass)` sets its own viewport, scissor, pipeline and bind groups and does not restore the host's
  state; this is documented in `Docs/Backends.md`.

## 6. Text, fonts and icons

- **Shaping.** HarfBuzz shapes UTF-8 runs (kerning and ligatures on). Runs are split by font fallback: requested
  font → Public Sans → Phosphor (icons live in the Private Use Area) → fonts added by the host, in order.
- **Fonts.** Public Sans is the default font, JetBrains Mono the embedded monospaced one (`GetMonospacedFont`).
  The font of a text is the per-call `TextOptions::Font`, else the innermost `PushFont`, else `Theme::Font`,
  else Public Sans. `GetTextSpec` returns the pushed or theme font, so every component follows `PushFont`.
- **Variable weight.** One FreeType face per font file; a weight instance (`wght` axis) per used weight, each with
  its own HarfBuzz font. `FontWeight` is a numeric 100–900 enum.
- **Glyph atlas.** A single-channel atlas with skyline packing, rasterized without hinting at
  `size × contentScale` pixels. Glyph key: face, weight, glyph index, pixel size, horizontal sub-pixel bin
  (4 bins). Baselines snap to whole pixels. The atlas starts at 512² and grows by doubling (to a 4096² cap);
  existing glyphs keep their texel coordinates, and glyph quads carry texel UVs that the shader normalizes, so
  quads emitted before a mid-frame growth stay valid. When the content scale changes or the cap is hit, the atlas
  is cleared at the start of the next frame and refilled lazily. Only dirty rows are uploaded.
- **Shaped-line cache.** Shaping happens in font units, so a shaped line is independent of size and content
  scale. Lines are cached by a hash of (text, font, weight, italic, icon variant) and evicted after about ten
  seconds without use, so steady-state frames do no shaping and no allocation.
- **Fallback.** Private Use Area code points use the icon font if it has the glyph; every other character uses
  the requested face if it has the glyph, then the other registered fonts in order. The monospaced font is never
  a fallback. A line is split into runs per face and each run is
  shaped separately.
- **Text drawing** is `DrawList::AddText`. It is declared on the draw list for convenience but implemented in
  `Text/`, because text sits above the draw list in the layering.
- **Type ramp** (`TextStyle`), from the HIG macOS table: Large Title 26/32, Title 1 22/26, Title 2 17/22,
  Title 3 15/20, Headline 13/16 bold, Body 13/16, Callout 12/15, Subheadline 11/14, Footnote 10/13,
  Caption 1 10/13, Caption 2 10/13 medium. `TextOptions::Emphasized` selects the HIG's emphasized weight.
  Public Sans is compared against SF Pro in M2 (x-height and advance widths); any size or weight adjustment is
  recorded in `Docs/Styling.md`.
- **Icons.** Phosphor regular, bold and fill fonts are embedded. `Carbon::Icons::House` etc. are
  `inline constexpr const char*` UTF-8 strings generated at build time from Phosphor's `selection.json` into the
  build tree, so icons can be drawn with `Icon()` or embedded in any label. The three fonts share their code
  points. Icon weight follows text weight (semibold and above use the bold font); `TextSpec::Icons` selects a
  variant explicitly. Icons are drawn at 1.2 × the text size and centered on the capitals of the primary font.

### Vulkan backend

In `Backends/Vulkan/`: `VulkanInit`, `VulkanShutdown`, `VulkanRender(commandBuffer)`,
`VulkanGetTextureID(view, layout)`, `VulkanReleaseTexture(view)` and `VulkanImage(...)`.

- The pipeline is built for the host's `VkRenderPass` and subpass, or, without one, for dynamic rendering from the
  formats in `VulkanInitInfo`. Push constants carry the frame uniforms; set 0 is the frame's primitive storage
  buffer, set 1 the texture of a draw command. Viewport and scissor are dynamic.
- Shaders: `Shaders/Carbon.vert` and `Carbon.frag` in GLSL 450, ports of the WGSL (Vulkan's clip space has y
  down). `glslc` compiles them at build time to SPIR-V word lists in the build tree, included by
  `VulkanRendererInternal.cpp`.
- Per frame in flight: host-visible, persistently mapped vertex, index and primitive buffers and the primitive
  descriptor set. The slot advances once per frame (`EndFrame`); objects that recorded frames may still use are
  retired and destroyed `FramesInFlight` frames later.
- The glyph atlas is an `R8_UNORM` image. Uploads go through a ring of staging buffers and command buffers with
  fences, submitted to the host's queue from `UpdateGlyphAtlas`, with barriers against earlier and later
  sampling. A new size gets a new image.
- `VulkanAllocatorInternal` hands out device memory from 4 MB blocks per memory type with a first-fit free list;
  buffers and images never share a block (no `bufferImageGranularity` concerns); large requests get a block of
  their own.

### OpenGL backend

In `Backends/OpenGL/`: `OpenGLInit`, `OpenGLShutdown`, `OpenGLRender()`, `OpenGLGetTextureID(texture)` and
`OpenGLImage(...)`, for OpenGL 3.3 core.

- `OpenGLFunctionsInternal.h` declares the GL types and constants the backend uses and a table of 55 functions,
  resolved through the host's `GetProcAddress`. No GL header, no loader library.
- One program (GLSL 3.30, or GLSL ES 3.00 for the OpenGL ES backend), one vertex array with Carbon's vertex and
  element buffers, an `RGBA32UI` 2D texture for the primitives (floats as bits, exact; 1024 per row), a sampler
  object for every texture, and an `R8` atlas texture.
  Buffers are orphaned with `glBufferData` every frame, so the driver never waits for the GPU.
- `OpenGLRender` saves the state it changes, draws into the bound framebuffer, and restores the state. Clip space
  and scissor rectangles are flipped for OpenGL's bottom-left origin; `GL_FRAMEBUFFER_SRGB` follows the sRGB-ness
  of the init info's format.

### OpenGL ES backend

`Backends/OpenGLES/OpenGLESBackend.h` (`OpenGLESInit`, `OpenGLESShutdown`, `OpenGLESRender()`,
`OpenGLESGetTextureID`, `OpenGLESImage`) installs the OpenGL renderer in ES mode, as its own type
(`OpenGLESRenderer`), so that each API finds only its own backend. ES mode compiles the shaders as GLSL ES 3.00,
loads no desktop-only function and leaves the state OpenGL ES does not have alone.

### Direct3D 11 backend

In `Backends/DX11/`: `DX11Init`, `DX11Shutdown`, `DX11Render(context)`, `DX11GetTextureID(view)` and
`DX11Image(...)`, for feature level 10.0 and later.

- The public header forward-declares the three D3D11 interfaces it names; the implementation calls only their
  methods, so Carbon links no Direct3D library.
- `Shaders/Carbon.hlsl` (vs_4_0, ps_4_0), a port of the WGSL, is compiled by `fxc` at build time into bytecode
  headers in the build tree. Primitives live in a dynamic `Buffer<uint4>` (floats as bits, read with `asfloat`),
  vertices and indices in dynamic buffers; all three are mapped with `WRITE_DISCARD` and grow by doubling.
- The glyph atlas is an `R8_UNORM` texture, updated by row range with `UpdateSubresource`.
- `DX11Render` saves the pipeline state it changes and restores it; it records into the init context or a context
  it is given, which may be deferred. Host views are held through `ComPtr` while in use.

### Direct3D 9 backend

In `Backends/DX9/`: `DX9Init`, `DX9Shutdown`, `DX9Render()`, `DX9InvalidateDeviceObjects()`,
`DX9GetTextureID(texture)` and `DX9Image(...)`, for shader model 3.0.

- `Shaders/Carbon.hlsl` (vs_3_0, ps_3_0) is compiled by `fxc` like the Direct3D 11 shaders. Primitives are copied
  into the vertices on the CPU (`DX9Vertex`, 56 bytes), because shader model 3.0 has neither integer attributes nor
  buffers a pixel shader can index. The vertex shader adds Direct3D 9's half-pixel offset.
- Vertex and index buffers (32-bit indices), the `L8` glyph atlas and the state block live in `D3DPOOL_DEFAULT`;
  `DX9InvalidateDeviceObjects` releases them before a device reset, and they are created again on demand.
- `DX9Render` captures a `D3DSBT_ALL` state block and applies it afterwards; it skips the frame while the device is
  lost, which leaves the atlas changes pending.

## 7. Layout algorithm

Layout is single-pass with one frame of latency for anything that needs a size it cannot know yet.

- A **container** (stack, scroll view, root) has an ID and a small persisted record: last frame's content size,
  resolved outer size, total flexible weight, and the frame it was last seen.
- `Begin*Stack` resolves the outer rect per axis: `Fixed(n)` → n; `Fill(w)` → its share of the parent's free
  space; `Fit` → last frame's measured content size plus padding. The stack is itself an item in its parent.
- Each child calls `AllocateItem(size)`. Along the main axis the child goes at the cursor, which then advances by
  the child's size plus spacing — no `SameLine()`. Across the axis the child is offset by
  `(containerCrossSize - childSize) × alignmentFactor`.
- **Free space** = container main size − last frame's sum of non-flexible children − spacing. It is divided among
  `Spacer()`s and `Fill` children by weight. With no flexible children, `Justify` offsets the whole content.
- `End*Stack` stores this frame's measurements for the next frame.
- **First-frame settle.** A container not seen on the previous frame lays out with stale (zero) measurements, so
  it is drawn fully transparent that frame and fades in over ~120 ms from the next one. `IsAnimating()` reports
  true while any container is unsettled, so event-driven hosts render the follow-up frame.
- **Stack identity.** A stack is identified by its call site (`std::source_location`), the enclosing container
  and the ID stack, or by `options.ID` when given. Call sites are stable when sibling stacks appear and
  disappear, which call order is not. Several stacks begun from one call site in the same frame (a loop) are
  told apart by their order. Stacks do **not** push onto the ID stack, so adding or removing a stack never
  changes widget IDs and widget state (focus, animation) survives layout refactors. *Decision.*
- **Backgrounds.** A stack's background must be drawn before its content but its size is known only after. The
  draw list reserves the quad at `Begin` (`AddDeferredSquircle`) and patches it at `End`, so the background
  has this frame's size with no lag.
- **Pixel snapping.** Item origins are snapped to whole pixels at the current content scale.
- **Fill.** A `Fill` item's share is purely proportional to its weight (its minimum is zero), so
  `Fill(1)` / `Fill(2)` panes are exactly 1 : 2 whatever they contain. `Fill` items do not count towards the
  measured content of a fitting axis, which keeps a full-width separator from inflating its stack.
- **ScrollView** is a container with a clip rect and a persisted, spring-animated scroll offset. The wheel goes
  to the innermost scroll view under the pointer, determined during the previous frame. The overlay indicator
  (a pill) appears while scrolling and when the view first appears, and fades out after about a second. It
  takes an explicit ID and pushes it on the ID stack.
- **Per-ID state** lives in `Core/State.h`: `GetState<T>(id, lifetime)` returns a zero-initialized, trivially
  copyable block. `Transient` state is dropped when a frame passes without it being requested (animations,
  measurements); `Persistent` state lives as long as the context (scroll offsets).

## 8. Animation model

```cpp
struct AnimationSpec
{
    static AnimationSpec Spring(float response = 0.3f, float dampingFraction = 1.0f);
    static AnimationSpec Ease(Easing curve, float duration);      // Linear, EaseIn, EaseOut, EaseInOut, cubic Bézier
    static AnimationSpec None();
    AnimationTrait Trait = AnimationTrait::Motion;                // Motion | Appearance (see reduce motion)
};

float Animate(ID id, float target, const AnimationSpec& spec = AnimationSpec::Spring());
Vec2  Animate(ID id, Vec2 target, const AnimationSpec& spec = AnimationSpec::Spring());
Rect  Animate(ID id, const Rect& target, const AnimationSpec& spec = AnimationSpec::Spring());
Color Animate(ID id, Color target, const AnimationSpec& spec = AnimationSpec::Spring());
void  SetAnimationValue(ID id, float value);                      // jump without animating
```

- **Springs** use the closed-form solution of the damped harmonic oscillator (under-, critically and over-damped
  branches), parameterized like SwiftUI: `response` (seconds) and `dampingFraction`. Each frame advances the
  stored `(value, velocity)` analytically by `dt`, so results are independent of how time is sliced into frames.
- **Interruptible.** Retargeting only replaces the target; value and velocity carry over. This is what makes a
  sidebar highlight glide when the user clicks quickly between rows.
- **Per-ID state.** The first call for an ID starts at the target (no animation on appear). State not touched for a
  few frames is dropped.
- **Defaults.** Controls use critically damped springs with responses of roughly 0.15–0.35 s; bounce
  (`dampingFraction < 1`) is used only where Apple does, e.g. the switch knob and sheet presentation.
- **Reduce motion.** `Motion`-trait animations (positions, sizes) jump to their target; `Appearance`-trait
  animations (colors, opacity) become a 150 ms ease. Components whose state change is a movement cross-fade.
- **Theme switch.** `SetTheme` starts a 350 ms transition; every style lookup reads the interpolated theme, so all
  colors and metrics animate without widget involvement.

## 9. Styling

```cpp
struct Theme
{
    static Theme Light();
    static Theme Dark();
    Color Colors[size_t(StyleColor::Count)];
    float Vars[size_t(StyleVar::Count)];
    TextStyleSpec TextStyles[size_t(TextStyle::Count)];
};
```

- `StyleColor` follows the macOS semantic colors: `Background`, `SecondaryBackground`, `ControlBackground`,
  `ControlFill`, `Label`, `SecondaryLabel`, `TertiaryLabel`, `QuaternaryLabel`, `PlaceholderText`, `Separator`,
  `Accent`, `OnAccent`, `Selection`, `UnemphasizedSelection`, `TextSelection`, `Destructive`, `FocusRing`,
  `OverlayBackground`, `OverlayBorder`, `Scrim`, `Shadow`, `ScrollThumb`, plus the system palette
  (`Red` … `Gray`) for charts.
- `StyleVar`: `CornerRadius`, `CornerSmoothing`, `Spacing`, `ControlHeight`, `ControlPadding`, `BorderWidth`,
  `FocusRingWidth`, `FocusRingOffset`, `DisabledOpacity`, …
- **Precedence**: per-call option > `PushStyleColor`/`PushStyleVar` stack > theme. One function implements it:
  `Resolve(std::optional<T> perCall, StyleColor|StyleVar)`. `EndFrame` asserts that every stack is balanced.
- **Dark**: `Background` #000000, `Label` #FFFFFF, grays modelled on Apple's dark label/fill colors, accent
  #0A84FF. **Light**: `Background` #FFFFFF with #F5F5F7 grouped surfaces, `Label` #000000, accent #007AFF.
  Theme colors are opaque; text contrast meets the HIG's 4.5:1 minimum.
- **Metrics** start from macOS regular control size (control height 24 pt, corner radius 6 pt, smoothing 0.6,
  focus ring 3 pt, 13 pt Body text) with `ControlSize::Small/Regular/Large`; values are tuned against Gallery
  screenshots in M4 and tabulated in `Docs/Styling.md`.
- Overlays (menus, popovers, sheets) separate from the background with an opaque elevated surface, a hairline
  border and an analytic soft shadow. This is not translucency or blur.

## 10. Input, focus and keyboard navigation

- `IO` holds display size, content scale, delta time, mouse state (position, buttons, wheel, click count), key
  state with repeat, modifiers and the text-input queue. Modifier keys are ordinary keys; `Key::Shortcut` is the
  platform shortcut modifier (Ctrl by default, configurable) used for copy/paste/select-all.
- **Focus.** Focusable widgets register in submission order. Tab / Shift+Tab move to the next/previous registered
  widget (resolved at `EndFrame`, applied next frame, wrapping). Carbon behaves like macOS with Full Keyboard
  Access on: every control is reachable.
- **Focus ring.** Accent-colored squircle ring outside the control, animated in. Shown only when focus arrived by
  keyboard, except in text fields, which always show it (macOS behaviour).
- **Activation.** Space and Enter activate the focused control. Enter with no focused button triggers the
  `IsDefault` button of the active scope. Arrow keys act inside controls (slider, segmented control, lists,
  menus). Escape dismisses the topmost overlay.
- **Focus scopes.** An overlay that holds the pointer (it is modal or dismisses on outside clicks) also holds
  the keyboard while it is the topmost one: every focusable item is tagged with the overlay it was submitted
  in, Tab and `FocusNext`/`FocusPrevious` cycle through the items of that overlay only, the default button
  beneath is ignored, and focus returns to where it was when the overlay closes.
- **Hit testing.** Each item under the pointer *claims* it during the frame; the last claim wins, except that a
  claim from a higher draw layer is never replaced by a lower one. The winner is the hovered item of the next
  frame. While an item holds the pointer (between press and release) nothing else is hovered.
- **Tab** is resolved at the start of a frame against the previous frame's list of focusable items, and the
  newly focused item asks its enclosing scroll views to reveal it.
- **Disabled** is a scope (`PushDisabled`) that components also open for their own `Disabled` option. It dims
  drawing through the draw list's opacity stack and removes the items from hit testing and Tab order.

## 11. Overlays

Popovers, menus, alerts, sheets and tooltips are not windows. They are submitted in the normal frame, drawn into
the `Overlay`/`Tooltip` layers and positioned against an anchor rect, flipped and clamped to stay on the display.

```cpp
void OpenOverlay(ID id);   void CloseOverlay(ID id);   void CloseCurrentOverlay();   bool IsOverlayOpen(ID id);
bool BeginOverlay(ID id, const OverlayOptions& options);   // .Anchor, .Placement, .Alignment, .IsModal, .HasScrim,
void EndOverlay();                                         // .DismissOnOutsideClick, .DismissOnEscape, .ShowsArrow
```

Open overlays form a stack. Each one draws in its own sub-layer of `DrawLayer::Overlay` (its depth in the
stack), so a later overlay is above an earlier one whatever the submission order, and closing an overlay closes
everything above it. An overlay whose `BeginOverlay` is not called during a frame closes by itself.

- **Layout.** The content is a floating container: laid out like a `VStack`, placed from the anchor and the
  size measured in the previous frame, outside its parent's flow and opacity. A new overlay is hidden for its
  first frame, then fades in; closing is immediate.
- **Pointer.** A modal overlay, or one that dismisses on outside clicks, submits a display-sized invisible
  button beneath itself: it takes every click that misses the overlay. `IsRectHovered` is false for anything
  under an overlay's surface, and for everything under an overlay that holds the pointer.
- **Keyboard.** See focus scopes in section 10. Escape closes the topmost overlay, one per key press.

The core provides this mechanism (`Carbon/Overlay/Overlay.h`, see [Overlays](Overlays.md)); the concrete
components (popover, menu, alert, sheet) live in `CarbonExtensions`.

## 12. Extension API (`#include <Carbon/Extension.h>`)

The supported surface for custom components — what `imgui_internal.h` is for Dear ImGui, but stable and
documented.

```cpp
// Identity and per-ID state
ID GetID(std::string_view label);   ID HashID(std::string_view label, ID seed);   void PushID(...);   void PopID();
template <typename T> T* GetState(ID id, StateLifetime lifetime = Transient);   // zero-initialized, trivially copyable

// Layout
Rect AllocateItem(Vec2 size, const ItemOptions& options = {});   // reserves space in the current container
Vec2 ResolveItemSize(Vec2 size, const ItemOptions& options = {});
Rect GetContentRect();   Rect GetLastItemRect();   Vec2 GetCursorPos();   void SetCursorPos(Vec2 position);

// Interaction
Interaction ButtonBehavior(ID id, const Rect& rect, const ButtonBehaviorOptions& options = {});
// Interaction { Hovered, Pressed, Clicked, DoubleClicked, Focused, FocusVisible }
DragInteraction DragBehavior(ID id, const Rect& rect, const DragBehaviorOptions& options = {});
bool IsRectHovered(const Rect& rect);   void SetLastItem(ID id, const Rect& rect, const Interaction& interaction);
void PushDisabled(bool disabled = true);   void PopDisabled();   bool IsDisabled();   void SetCursor(Cursor cursor);
bool IsKeyPressed(Key key, bool repeat = true);   Vec2 GetMousePos();   // ... Input.h

// Focus
void RegisterFocusable(ID id, const Rect& rect);
bool IsFocused(ID id);   bool IsFocusVisible(ID id);   void SetFocus(ID id, bool showRing = false);   void ClearFocus();
void FocusNext();   void FocusPrevious();
void DrawFocusRing(ID id, const Rect& rect, float cornerRadius, bool alwaysWhenFocused = false);

// Drawing and text
DrawList& GetDrawList();                        // PushLayer / PopLayer select the layer
Vec2 MeasureText(std::string_view text, const TextSpec& spec);   TextSpec GetTextSpec(TextStyle style);
void DrawLabel(DrawList&, const Rect&, float x, std::string_view text, const TextSpec&, Color);
void DrawIcon(DrawList&, Vec2 center, std::string_view icon, float size, Color, IconVariant);

// Style (applies the precedence rules)
Color GetStyleColor(StyleColor color);   float GetStyleVar(StyleVar var);
Color Resolve(const std::optional<Color>& perCall, StyleColor fallback);
float Resolve(const std::optional<float>& perCall, StyleVar fallback);
ControlMetrics GetControlMetrics(ControlSize size);
ControlFeedback AnimateFeedback(ID id, bool isHovered, bool isPressed);   Color ApplyFeedback(...);

// Frame
float GetDeltaTime();   double GetTime();   Vec2 GetDisplaySize();   ContentScale GetContentScale();
void RequestAnimationFrame();

// Animation: Animate(...) from section 8.  Overlays: section 11.
```

There is no separate "internal" header for component authors: the list above is what `CarbonExtensions` itself
is built from. Two CTest cases (`PublicApiBoundary.*`) scan the sources of `Extensions/` and of
`Examples/CustomComponent` and fail on any include of a Carbon header that is not in the list of public headers.

`Examples/CustomComponent` builds a star-rating control with exactly this API, and
[CustomComponents](CustomComponents.md) walks through it.

## 13. Logging and asserts

- `Callbacks.Log(LogLevel, source, message)`; with no callback, logs are dropped. Messages use `std::format`.
- `CB_VERIFY(condition, "format", args...)` is active in every build type and guards against API misuse
  (unbalanced stacks, calls outside a frame). On failure it formats the message, logs at `LogLevel::Fatal` with
  source `Assert` and calls `Callbacks.AssertFailed` if the host set it. With that hook set (tests do this),
  execution continues and Carbon recovers; without it, debug builds break into the debugger (`__debugbreak` on
  MSVC, `__builtin_trap` on GCC/Clang) and release builds continue after logging.
- `CB_ASSERT` has the same form but checks internal invariants and compiles away outside debug builds unless
  `CARBON_FORCE_ASSERTS` is on.

## 14. Build, packaging and repository

- Root `CMakeLists.txt`: options and `add_subdirectory` only. Targets `Carbon` (`Carbon::Carbon`),
  `CarbonExtensions` (`Carbon::Extensions`) and `CarbonReflection` (`Carbon::Reflection`, built when
  `CARBON_BUILD_REFLECTION` and `CARBON_BUILD_EXTENSIONS` are on), all static.
- `CARBON_DEPS_<NAME>_BUILD` / `CARBON_DEPS_<NAME>_NAME` for FreeType, HarfBuzz, GoogleTest, GLFW and stb. Dawn
  is found only for the WebGPU backend (`find_package(Dawn CONFIG)` unless `CARBON_DEPS_DAWN_NAME` already
  exists). `CARBON_BACKEND_<NAME>` selects the backends compiled into `Carbon`; each exports
  `CARBON_HAS_BACKEND_<NAME>` and makes its graphics library a public dependency.
- Fonts and shaders are converted to `.cpp` byte arrays at build time by `Framework/CMake/EmbedAsset.cmake`
  (pure CMake, no Python), written to the build tree and never committed. `Icons.h` is generated the same way.
- Submodules pinned to release tags: FreeType, HarfBuzz, GoogleTest (v1.18.0), GLFW (3.5.1),
  Public Sans (v2.001), JetBrains Mono (v2.304), Phosphor web (v2.1.2); stb has no tags and is pinned to a
  commit.
- **Dawn**: developed against commit `91158020c0b1cb0ddb4dc1c2c29e5a4669374f0b` (2026-09-03). That install has
  no `webgpu_glfw` helper, so the examples create their surface per platform (Win32, X11, Wayland) in
  `Examples/Common/Devices/WebGPUSurface.cpp`.
- Examples accept `--screenshot <file.png>`, `--theme light|dark`, `--scale <factor>` and
  `--size <width>x<height>`; screenshot mode renders one settled frame to an offscreen texture and never opens a
  window. For screenshots of states that need input there are `--page <name>` and `--show <name>` (Gallery) and
  `--pointer`, `--click` and `--right-click <x>x<y>`, which script the pointer.
- CI: GitHub Actions on Windows (MSVC) and Linux (GCC, Clang), and an Emscripten build whose tests run in Node;
  Dawn is built once per pinned commit and cached.
- Benchmarks: `Benchmarks/` builds `CarbonBenchmarks` on Google Benchmark (v1.9.5, a submodule) when
  `CARBON_BUILD_BENCHMARKS` is on. [Optimizations](Optimizations.md) holds what they measure, how to run them
  and the numbers of every round of optimization.
- Emscripten: `CB_PLATFORM_WEB`; WebGL 2 through the OpenGL ES backend; the examples' GLFW is Emscripten's GLFW 3.4
  port.

## 15. Milestones

| Milestone | Content | Check |
| --- | --- | --- |
| M0 Scaffold | Repo files, submodules, CMake skeleton, `CLAUDE.md`, one trivial test | Configure + build; Dawn found or clear error |
| M1 Core | Platform, log, asserts, math, IDs, context, IO, content scale, draw list | Unit tests |
| M2 Rendering and text | Dawn renderer, squircles, FreeType/HarfBuzz, atlas, fonts, icons, MinimalIntegration | Screenshots, both themes, scale 1.0 and 2.0 |
| M3 Layout and animation | Stacks, Spacer, size modes, ScrollView, springs, easing, reduce motion, theme transition | Layout and animation tests |
| M4 Styling, input, core widgets | Themes, style stacks, option structs, keyboard navigation, focus rings, core widgets, Gallery | Gallery screenshots vs. HIG; interaction tests |
| M5 Extensions | Extension API final, overlays, all extension components, CustomComponent | Extensions build against public headers only |
| M6 Docs, CI, polish | Complete docs, README screenshots, GitHub Actions, HIG and naming sweep | CI green; fresh clone builds the Gallery |
| M7 Fonts | JetBrains Mono embedded, `GetMonospacedFont`, `TextOptions::Font`, `PushFont` / `PopFont` | Precedence, nesting and unbalanced-push tests; monospaced measurement |
| M8 Reflection core | `Reflection/` library and `CARBON_BUILD_REFLECTION`; enum and aggregate reflection, labels, `CB_REFLECT_*` macros; boundary checks | Unit tests on MSVC, GCC and Clang (CI) |
| M9 Reflect | `Carbon::Reflect` for enums and structs, `ReflectOptions`, field mapping, both layouts | Widget tests (change detection), zero allocations, screenshots in both themes at scale 1 and 2 |
| M10 Gallery page | "Reflection" page: five sections, each a code box above the generated UI | Screenshots vs. HIG |
| M11 Example and docs | `Examples/Reflection` settings window, `Docs/Reflection.md`, `Docs/Components/Reflect.md`, packaging (`find_package(Carbon COMPONENTS Reflection)`) | Package test; CI green |
| M12 Backend-neutral types | `Carbon::TextureFormat` and its queries | Unit tests |
| M13 Backend interface | `RendererBackend`, install and remove, core dispatch; the Dawn renderer behind the interface | Headless tests with a recording backend; renderer tests unchanged |
| M14 Backend build and tests | `CARBON_BACKEND_*` options, conditional dependencies, backend test harness, smoke and pixel-comparison tests | Tests on WebGPU |
| M15 WebGPU backend | Renderer and WGSL in `Backends/WebGPU/`, `WebGPU*` API, graphics-free core, `WebGPUMinimalIntegration`, `Docs/Backends.md` | Screenshots unchanged; isolation test |
| M16 Vulkan backend | `Backends/Vulkan/`, GLSL 450 compiled to SPIR-V at build time, `VulkanMinimalIntegration` | Smoke and pixel comparison against WebGPU; validation layers clean |
| M17 OpenGL backend | `Backends/OpenGL/`, GLSL 330, private function table, `OpenGLMinimalIntegration` | Smoke and pixel comparison against WebGPU |
| M18 OpenGL ES and the web | `Backends/OpenGLES/` over the OpenGL renderer, Emscripten build, `OpenGLESMinimalIntegration` in a browser | Smoke and pixel comparison against WebGPU; tests in Node |
| M19 Direct3D 11 backend | `Backends/DX11/`, HLSL compiled by `fxc`, `DX11MinimalIntegration` | Smoke and pixel comparison against WebGPU; debug layer clean |
| M20 Direct3D 9 backend | `Backends/DX9/`, shader model 3.0, device resets, `DX9MinimalIntegration` | Smoke and pixel comparison against WebGPU, also with the objects released every frame |
| M21 Examples per backend | `Examples/Common` over one graphics device per backend; `<Backend>Minimal`, `<Backend>Gallery`, `<Backend>CustomComponent`, `<Backend>CustomTitleBar` (not in a browser) | Every backend's screenshots identical to WebGPU's; documentation images unchanged |

## 16. Decision log

| # | Decision | Reason |
| --- | --- | --- |
| 1 | Per-call structs are `<Widget>Options` | `TextStyle` is the type-ramp enum in the brief's own example |
| 2 | Squircle = superellipse corner patch, apex-matched to the circular arc | Analytic in the fragment shader, exact circle at smoothing 0, zero curvature at the joins |
| 3 | Stacks are identified by their call site and do not push IDs | Widget state must survive layout changes; call order would make a stack "new" whenever a sibling before it appears |
| 4 | `ContextDescription` carries `DepthStencilFormat` and `SampleCount` | The pipeline must match the host's pass |
| 5 | Draw list stores an opaque `TextureID`; `wgpu::` types appear only in `ContextDescription`, `Render` and the `Image` overload | Keeps everything above `Renderer/` GPU-free. Replaced by decision 105 |
| 6 | Gamma-space blending on `Unorm` targets, linearized on `UnormSrgb` | Matches the look of macOS UI on the common surface formats |
| 7 | Carbon behaves as if Full Keyboard Access is on; focus ring only after keyboard focus (always for text fields) | The brief requires full keyboard operability; ring behaviour follows macOS |
| 8 | Shortcut modifier is Ctrl by default (configurable) | Targets are Windows and Linux |
| 9 | Overlays use opaque surfaces, hairline border and an analytic shadow | Separation on pure black without translucency or blur |
| 10 | Public/internal boundary enforced by an include check and by building extensions against the installed package | The source tree has a single include root |
| 11 | One draw list with four layers (shared vertices, per-layer indices and clip stacks) instead of one list per layer | No vertex patching when merging; `PushLayer`/`PopLayer` replaces `GetDrawList(layer)` |
| 12 | UTF-8 helpers live in `Core/`, not `Text/` | `Input` needs them and must not depend on `Text` |
| 13 | Misuse checks use `CB_VERIFY` (always on) and recover; `CB_ASSERT` is debug-only | Unbalanced stacks must be reported in release builds too, and tests run in both |
| 14 | `IO::AddMouseLeaveEvent` added | Hover must end when the pointer leaves the host's area |
| 15 | `CreateContext` makes the new context current only if none is current | Matches Dear ImGui and avoids surprising multi-context hosts |
| 16 | Glyphs are rasterized without hinting | Faithful shapes and spacing at every size, as on macOS; sub-pixel positioning keeps small text even |
| 17 | Glyph quads carry texel UVs | The atlas can grow mid-frame without invalidating quads already emitted |
| 18 | Every field of an option struct has a default member initializer | GCC `-Wextra` warns about omitted fields in designated initializers otherwise |
| 19 | Examples and tests copy `d3dcompiler_47.dll` next to their executables on Windows | Dawn only looks there unless built with `DAWN_FORCE_SYSTEM_COMPONENT_LOAD` |
| 20 | `MinimalIntegration` draws through the draw list until widgets exist | M2 has no layout or widgets yet; the example is rewritten with them in M4 |
| 21 | The type ramp and the font are part of `Theme` | Hosts customize typography the same way as colors; `GetTextSpec` reads the current theme |
| 22 | A theme switch interpolates colors and metrics but switches the type ramp at once | Interpolating sizes and weights would rasterize every glyph at every intermediate value |
| 23 | Colors animate premultiplied and always count as a change of appearance | Fades to and from transparent keep their hue; reduce motion keeps them as cross-fades |
| 24 | `IsAnimating()` also covers unsettled layout and fading containers | One call tells an on-demand host whether another frame is needed |
| 25 | An item is hovered only if it was the topmost claimant of the pointer in the previous frame | Later (or higher-layer) items win, so a button on a clickable row, or a popover over content, takes the pointer without any "allow overlap" flags |
| 26 | A mouse press that arrives in the same frame as a move is applied one frame later | Decision 25 needs the pointer to be in place for a frame before a press can be attributed |
| 27 | `ControlFill` is a translucent gray | Controls must read as controls on white and on grouped backgrounds; this is how Apple's system fills work. Materials stay opaque. |
| 28 | `Slider` and `TextField` do not draw their label; `Toggle` and `Button` do | A slider or field usually sits in a row with its own title; the label is the ID, and the field's placeholder |
| 29 | Text fields report Enter through `IsItemSubmitted()` | The return value stays "the text changed", consistent with every other value widget |
| 30 | The default button reacts to Enter one frame later | Whether a focused control uses Enter is only known once the whole frame has been submitted |
| 31 | Word wrap, truncation and alignment are part of `TextSpec` | `MeasureText` and `AddText` keep one signature; the shaping cache is unaffected |
| 32 | `DrawLayer::Overlay` is a stack of eight sub-layers, one per open overlay | A menu opened from a popover must draw above it even when it is submitted first; the public enum keeps four layers |
| 33 | Overlays capture the pointer with an invisible display-sized button beneath them, not with a special input mode | It reuses the hover-claim rule (decision 25): the highest layer wins, so nothing below needs to know about overlays |
| 34 | Overlays close without an animation | A closed overlay is no longer submitted, so there is nothing to draw a fade-out from; macOS menus and popovers also disappear almost at once |
| 35 | `OpenOverlay` and `BeginOverlay` take an `ID`; the components built on them take a label | The ID must be the same at both calls, so components document "call Open at the same ID scope as Begin" and offer `CloseCurrent...` for use inside |
| 36 | The helpers in `WidgetInternal.h` became the public `ControlFeedback.h`; `DrawIcon`, `FocusNext`/`FocusPrevious`, `RequestAnimationFrame` and frame accessors were added | Writing the extensions showed what a component author needs; nothing in `CarbonExtensions` uses an internal header |
| 37 | The extension boundary is checked by scanning includes (CTest `PublicApiBoundary.*`) | Cheap, runs everywhere, and catches relative includes too; building against the installed package follows in M6 |
| 38 | Sidebar, List and Table do not own the selection: a row reports that it was picked and the application marks rows as selected | Immediate mode: the application owns the data. Arrow keys therefore take effect one frame later, through the row they move to |
| 39 | A submenu is a modal overlay whose outside clicks are interpreted by the menu chain | The parent menu must stay open but inert while the pointer is in the submenu; the chain decides whether a click closes one level or all |
| 40 | A pop-up button's menu opens with the current item over the button | macOS behaviour; the menu is anchored at a point computed from the row metrics |
| 41 | Sheets are centered cards over a scrim, not attached to a title bar | macOS 11+ look, and Carbon has no window chrome to attach to; `OverlayPlacement::Top` exists for hosts that want the older look |
| 42 | Charts draw lines as round-capped segments and bars as squircles clipped at the zero line; no polygon primitive was added | Keeps the renderer at one quad per shape; filled areas under lines are out of scope |
| 43 | The examples accept `--page`, `--show`, `--pointer` and `--click` | Screenshots of hover states, menus and dialogs can be taken unattended |
| 44 | The installed package exports bundled FreeType and HarfBuzz as `Carbon::freetype` and `Carbon::harfbuzz` | A static Carbon needs them at link time; installing their archives next to Carbon's keeps `find_package(Carbon)` self-contained without merging archives |
| 45 | `Tests/Package` is a separate CMake project that consumes the installed package | It is the only way to prove that custom components and the extension sources compile from the installed headers alone |
| 46 | CI builds Dawn once per platform and caches the install by commit; the Linux jobs share one Dawn built with Clang | Dawn dominates CI time, and at the pinned commit it does not compile with GCC 13. GCC and Clang both use libstdc++, so one static library serves both |
| 47 | Hover and pressed tints are composited over the fill (`Blend` is source-over), and every interactive control reacts to the pointer | Mixing only the color of the 16 % control fill was measurable but invisible. macOS itself gives few controls a hover state; Carbon's users are on Windows and Linux, where a control that does not react feels dead |
| 48 | The selection highlight of Sidebar, List and Table survives one frame without a selected row | An application that selects in response to a click changes its selection mid-frame; when the new row precedes the old one, no row is selected in that frame, and the highlight would vanish and then jump |
| 49 | Custom title bars are example code, not a component; Carbon gained only two diagonal resize cursors | The window side is OS integration and stays in the host. The user chose to keep the title bar in the example; the cursors are needed by any host that resizes from corners |
| 50 | OutlineView items are always Begin/End pairs, with `IsExpanded` in the returned struct; leaves too | One shape for every item keeps recursive code simple, and each item can push its ID for its children, so labels need only be unique among siblings |
| 51 | ColumnView leaves the path to the application; Carbon draws, scrolls and moves the focus | Immediate mode: the hierarchy and the selection are the application's data, as with List and Sidebar |
| 52 | MenuBar is an in-window bar in Carbon's style; Alt or F10 open it, the arrows move along it | Carbon draws into the host's window and has no screen-wide menu bar to join. Alt and F10 are what Windows and Linux users expect |
| 53 | ComboBox filters its list while typing, instead of completing inline | Inline completion needs the text field to select the completed part, which would add API for one component; a filtered list serves the same purpose and is common on the target platforms. TextField gained `TrailingInset`, `VerticalArrowsMoveCaret` and `ReloadTextField`, and the interaction API `SetItemSubmitted` |
| 54 | Notifications are posted from anywhere and drawn by one `ShowNotifications` call per frame; their text is copied into fixed buffers | Posting must work from code that has no part in building the interface (a finished job). Fixed buffers keep the state trivially copyable and make showing them allocation-free |
| 55 | Notifications draw in the top overlay sub-layer and take clicks there, but never the keyboard | They must stay visible over sheets and popovers, and must not interrupt typing |
| 56 | `Grid` and `GridRow` are container kinds of the stack layout: a row places its items into cells inside the same three placement steps that every widget and nested stack already goes through | Widgets, stacks and nested grids work in cells without any change of their own, and grids inherit call-site identity, the first-frame settle and `IsAnimating()` from the stacks |
| 57 | Column widths are measured in one frame and used in the next, kept in a fixed 32-column record; span and alignment of a cell are set with `SetNextGridCell` rather than new `ItemOptions` fields | The same one frame of latency as a fitting stack; per-ID state must be trivially copyable; every widget would otherwise have to forward cell options |
| 58 | A `Fill` cell takes its column's width, but its column measures the cell's content | A column of only `Fill` cells (equal-width chips) would otherwise have no width at all, and a cell that measured its stretched width would keep its column from ever shrinking |
| 59 | A radio group is one Tab stop whose arrow keys change the selection directly; -1 means no button is selected | This is AppKit's behaviour with Full Keyboard Access; a group that starts without a choice is a legitimate state for radio buttons, unlike segmented controls |
| 60 | `PathControl` returns the index of the activated component (-1 for none) instead of a `bool` with a value it edits | The application owns the path (decision 51); what the control reports is an action on one of its components, not a new value |
| 61 | A path that does not fit hides the middle names first, from the root's side, then truncates the root's name and last the selected item's; a hovered or highlighted component always shows its name. The pop-up style's menu lists the path from the selected item down, without check marks | The HIG names only the first step; keeping the selected item readable longest matches Finder's path bar. The menu has one leading column, which a check mark would take from the icons |
| 62 | A toolbar decides which items fit from the widths of the previous frame and moves the rest, from its trailing end, into an overflow menu; an item chosen there reports it in the next frame | Whether an item fits is known only after every item has been submitted. Items are copied into fixed buffers so the menu can be built at the end, as with notifications (decision 54); the one frame of latency is that of decision 38 |
| 63 | A control in the overflow menu opens in a popover below the chevron and gets the keyboard; with labels shown, controls get their label below them | A menu cannot hold a search field. macOS shows the labels of all toolbar items, controls included, in the icon-and-label mode |
| 64 | `ToolbarOptions::Height` is optional (52 points with labels, 38 without) where `MenuBarOptions::Height` is a fixed default | The right height depends on the display mode, which the options also choose |
| 65 | Dates are a trivially copyable `DateTime` (year, month, day, hour, minute) of local wall time, shared by both date components in `DateTime.h`; calendar arithmetic goes through `std::chrono`, and the operating system's time zone is used only to read the current time | The application owns the value and decides what it means; time zones are out of scope. `std::chrono`'s calendar types do the leap-year and weekday arithmetic, and a plain struct of ints works with designated initializers and per-ID state |
| 66 | The calendar always shows six weeks; its month buttons are not Tab stops (Page Up and Page Down move by month), so it is one stop; the month on display follows the value whenever the value changes | A fixed height keeps the layout from jumping between months. One stop keeps the calendar usable inside DatePicker's popover. The month on display is view state, the value stays the application's |
| 67 | `DatePicker`'s field is drawn and edited by the component, element by element, not built on `TextField`; complete elements move the selection on, and a separator typed right after that only confirms the move | macOS's textual date picker edits year, month, day, hour and minute separately; free text would have to be parsed and could be invalid while typing. Ignoring the confirming separator is how typing "2027-12" works on macOS |
| 68 | The date picker's calendar popover leaves the keyboard in the field and closes when the field loses focus, on a click outside, on Escape or Enter, and after a day is picked with the mouse | The ComboBox precedent (decision 53): typing goes on while the calendar shows the result. Closing for lost focus waits until the popover has been open for a frame, because focus requests take effect a frame late |
| 69 | The stepper is Carbon's `Stepper` bound to a scratch value between -1 and 1, and moves the selected element | It brings the stepper's look, repeat and keyboard behaviour without a second implementation |
| 70 | `DateFormat` is a struct with `ISO()`, `German()` and `US()` presets, and `FormatDateTime` is public | Formats are the application's choice (no locales); the presets cover the required styles and a format can be adjusted field by field. Applications can show a value as the picker does, without allocating |
| 71 | `TextField` gained `IsBezeled`, `AcceptsInput` and `GetTextFieldSelection` | A token field needs a borderless field after its tokens, must take Backspace, Delete and the arrows while tokens are selected, and must know when the caret is at the start of the text. The editor itself stays internal; this follows `TrailingInset` (decision 53) |
| 72 | The text a token field is still editing lives in its per-ID state as a fixed 512-byte buffer, bridged to the `TextField` by one reused scratch string; it becomes a token at a delimiter, at Return and when the field loses focus | Per-ID state must be trivially copyable, and the application's list holds only finished tokens. Copying in and out of a reserved string costs no allocation, and each field keeps its own text. `NSTokenField` also tokenizes when editing ends |
| 73 | Token fields tokenize at a comma (configurable) and at Return; there are no suggestions. Typing while tokens are selected replaces them, and a right click on a token opens a menu the application builds with `BeginTokenFieldMenu` | The HIG names the comma as the default and Return as a common addition, calls suggestions optional, and recommends a context menu on tokens. Replacing the selection is how Mail behaves |
| 74 | Documentation screenshots are listed in `Docs/Images/Screenshots.txt`, rendered by `Scripts/Screenshots.py`, and committed by CI after every push to `main` when they look different; the Gallery can crop to its sections (`--section`) and shows a fixed day as today in screenshots | Images that are rendered from a list cannot fall behind the code. Cropping to a section's box follows layout changes, where fixed pixel areas would not. WARP in CI renders within a level or two of a GPU, so a small tolerance keeps unchanged images from being committed again |

| 75 | JetBrains Mono is embedded as Carbon's monospaced font and is not a fallback for other fonts | Code needs a monospaced face without a file; as a fallback it would change how missing characters of proportional text look |
| 76 | Private Use Area characters take the icon font before the requested font | JetBrains Mono has powerline glyphs at four of Phosphor's code points; `Carbon::Icons` must draw the same icon in every font |
| 77 | `PushFont` is part of the style stack, and `GetTextSpec` returns the pushed font | Every core and extension component builds its text from `GetTextSpec`, so all of them follow a pushed font without changes |
| 78 | Reflection is a third library, `CarbonReflection`, on top of `CarbonExtensions`; `CARBON_BUILD_REFLECTION` without the extensions skips it with a message | It draws with extension components; applications that do not want it pay nothing |
| 79 | The headers the reflection templates are made of live in `Detail/` and are installed; they use `Carbon::Internal` | Templates are compiled in the application, so their helpers must be reachable from the installed headers. `Internal/` keeps its meaning: never installed |
| 80 | `CB_REFLECT_*` define a function `CarbonReflectDescribe(TypeTag<T>)` next to the type, found by argument-dependent lookup, instead of specializing a Carbon template | A specialization must be written in an enclosing namespace of `Carbon` (in practice the global one); the function works in any namespace, including one opened to describe someone else's type |
| 81 | The options of `CB_FIELD` / `CB_VALUE` are a braced initializer and required (`{}` when empty); `ReflectFieldOptions` is ordered DisplayName, Min, Max, Step, Control, Tooltip, Hidden, ReadOnly | The braces carry commas through the preprocessor without `__VA_OPT__`, so the macros also work with MSVC's traditional preprocessor in applications. The order lets the brief's examples compile, since designated initializers follow declaration order |
| 82 | Enum values are sorted by value; hidden values are left out of the count; values with one number are one value | A stable order for controls; `Count` sentinels should not be selectable |
| 83 | Labels are formatted at run time, once per type, into a static table | The formatting code stays out of the templates (one `.cpp`), no frame allocates, and labels and names stay `std::string_view` |
| 84 | Field names come from a pointer to the field of an `extern` object that is never defined, wrapped in a class-type template argument; field counts from brace initialization with a type that converts to anything | MSVC only evaluates the pointer when the object is reached through a function parameter; Clang spells a bare subobject pointer without the field's name. These are the Boost.PFR techniques, written in-house |
| 85 | Automatic reflection supports up to 64 fields and the enum range [-128, 127], clamped to the underlying type; a macro range may span 1024 values | Each limit trades compile time for coverage; 64 fields and 256 values cover settings structs and enums, with the macro for the rest |
| 86 | `ReflectControl` also has `Switch` and `Checkbox` | macOS settings use both for booleans; the field's control override is the natural place to choose |
| 87 | Private fields cannot be reflected, even with the macro | The type is never modified, and only a `friend` declaration inside the type could grant access |
| 88 | A `DateTime` field gets a date picker for the date only | `ReflectFieldOptions` has no elements to choose, and settings dates are usually days; an application that needs the time draws the field itself or hides it |
| 89 | Floating-point values are formatted with `std::to_chars`, integers with `std::format_to_n` | MSVC's `std::format` allocates for a floating-point value with a precision, and a settled `Reflect` frame must not allocate |
| 90 | A checkbox field carries its label after the box and leaves the label column empty; the value of a number sits after a slider and before a stepper | That is how macOS forms place them; the value's column has a fixed width so the control beside it does not move |
| 91 | The tooltip of a field is attached to its whole row with `SetLastItem` | Containers do not register as items for `Tooltip`; the extension API already allows this |
| 92 | Every field of a struct is reached with a comma fold, never with `|` or `||` | Fields must be drawn in order, every frame; `|` leaves the order of evaluation unspecified |
| 93 | Automatic labels use title-style capitalization: minor words (articles, conjunctions, short prepositions) inside a label stay lowercase | The HIG asks for title-style labels; "Launch At Login" reads like a machine wrote it |
| 94 | The Reflection example's executable is called `Reflection`, its target `ReflectionExample` | Every example's executable is named after its folder; a target called `Reflection` would read like the library |
| 95 | `Carbon::TextureFormat` names the formats of a host's render targets without a graphics API: the 8-bit RGBA and BGRA formats with their sRGB variants, RGB10A2, RGBA16Float, and the depth-stencil formats. Depth formats are named exactly (`Depth24Unorm`, not "at least 24 bits"); the sample count stays a `uint32_t` | Carbon is getting renderer backends for several APIs, and core headers must not include any of them. A Vulkan or Direct3D 12 pipeline needs the exact depth format of the pass; a backend whose API is vaguer (WebGPU's `Depth24Plus`) maps to its closest format. A number needs no type of its own |
| 96 | Rendering goes through a public interface, `RendererBackend`, that core pushes into: a backend's own render function stores its native target and calls `RenderDrawData()`, and core calls `UpdateGlyphAtlas` and `Render`. No native handle is part of the interface | Each API needs different handles (a pass encoder, a command buffer, nothing at all), so they belong to the backend's own functions. Pushing keeps the order of operations, and everything in decision 97, in one place instead of in every backend |
| 97 | Glyph-atlas tracking (generation, dirty rows, the first full update), the host-texture registry and the "render after `EndFrame`" check are core code; a backend only copies the rows it is given and drops a texture when told | Three backends would otherwise carry three copies of the same rules, and the rules are what hosts rely on |
| 98 | A backend is called to do GPU work (`UpdateGlyphAtlas`, `Render`, `ReleaseTexture`) only from `RenderDrawData`, `FlushGlyphAtlas`, `ReleaseHostTexture` and its own destruction. `NewFrame` and `EndFrame` only notify it (`BeginFrame`, `EndFrame`) and queue expired textures | During `NewFrame` an OpenGL context need not be current, and a Direct3D 11 or Metal host may be on another thread than the one that renders |
| 99 | A host texture counts as used when it is registered or when the frame's final draw data draws it (`EndFrame`), no longer when it is rendered | A host that builds frames without rendering them (a minimized window) would lose its textures, and the rule no longer depends on a backend |
| 100 | The interface was checked against what later backends need, and has four things only they use: `RendererBackendCapabilities::MaxTextureSize` (OpenGL ES 3.0 and old Direct3D 9 hardware guarantee 2048, the atlas grew to 4096 unconditionally), `FlushGlyphAtlas` (Vulkan, Direct3D 12 render passes and Metal cannot upload where the frame is drawn, so a backend can offer an earlier call), `InvalidateGlyphAtlas` (a Direct3D 9 device reset or a lost OpenGL ES context loses the texture) and `RendererBackendVersion` (a backend outside the repository must not silently draw a new primitive kind wrong). Direct3D 12 descriptor heaps, frames in flight, root signatures and resource states, and nvrhi's command lists and framebuffers, are init-info fields, arguments of the backend's own functions or backend-internal state. OpenGL ES 3.0 has no texture buffers and Direct3D 9 no integer vertex attributes; both are served by the raw draw data, which a backend may repack (primitives in a float texture, or expanded into the vertices) | Adding a backend later must not change the interface, or every backend outside the repository breaks |
| 101 | A context owns its one backend (`std::unique_ptr`); the destructor is the shutdown, and `DestroyContext` makes the context current while it runs. `GetRendererBackend<T>()` identifies the type by the address of a per-type tag recorded at install, not by name or RTTI | One owner and one way to shut down. A backend's functions need their object back safely: names can collide with a user's backend, and Carbon does not require RTTI. The tag is not `const` because linkers may merge identical constants |
| 102 | A backend's device limit also shrinks an atlas that is already larger: it is cleared and rebuilt | Installing a backend after text was laid out must not leave an atlas the device cannot hold |
| 103 | `CARBON_BACKEND_<NAME>` are declared in `ThirdParty/CMakeLists.txt`, next to the search for their dependency, not in the root file; unset, each defaults to whether its dependency was found, and the result is cached | Their default depends on that search, which a plain `option()` in the root cannot express. Caching keeps a configuration from switching a backend off silently when a dependency goes missing |
| 104 | Renderer tests are written once against a `BackendHarness` (create a device without a window, install the backend, render offscreen, read back, make a texture) and run per compiled-in backend as value-parameterized tests. They fail on any warning or error Carbon logs, any failed check and any message of the API's validation. Pixel comparisons use WebGPU as the reference, which is also compared with a second rendering of its own | One set of tests holds every backend to the same behavior. Logged warnings are how backends report misuse, so a clean log is part of passing. A reference that is not stable would make every comparison meaningless |
| 105 | Every backend lives in `Framework/src/Carbon/Backends/<Name>/` with its public header (`<Name>Backend.h`), its implementation and its shaders, and has the API `<Name>Init(const <Name>InitInfo&)`, `<Name>Shutdown()`, `<Name>Render(...)`, `<Name>GetTextureID(...)` and `<Name>Image(...)` as free functions in `namespace Carbon`. `ContextDescription` lost its device, formats and sample count; `Carbon::Render`, `Carbon::GetTextureID`, the `Image(wgpu::TextureView)` overload and `GetEmbeddedShader` were removed without shims | One prefix per backend reads like the rest of Carbon's free functions, and a host that includes one backend's header sees no other API. A context is created without knowing the API, so formats belong to the backend's init info. Carbon is before 1.0, and shims would keep `wgpu::` in core headers. This replaces decision 5 |
| 106 | `<Name>Init` returns `bool` and logs why it failed; `<Name>Render` and `<Name>GetTextureID` without that backend installed fail a check | A host can fall back to another backend when one cannot start, which is a run-time condition. Rendering with a backend that is not installed is a programming error |
| 107 | The main examples stay on WebGPU through `Examples/Common`, now split into a backend-neutral library (arguments, GLFW input, PNG screenshots) and the WebGPU host. `MinimalIntegration` became `WebGPUMinimalIntegration`; the main examples are skipped with a message when the WebGPU backend is off | The minimal examples of the other backends share the neutral part and stay self-contained otherwise. The Gallery's images are WebGPU textures |
| 108 | The Vulkan backend links the loader (`Vulkan::Vulkan`) and calls Vulkan functions directly, uses its own allocator rather than VMA, and compiles GLSL 450 to SPIR-V with `glslc` at build time into word lists (`-mfmt=num`) that a source file includes | No new dependency besides the SDK the backend needs anyway. Carbon allocates little and rarely, so a block allocator is enough. Including words needs no embedding script and keeps SPIR-V out of the repository |
| 109 | Vulkan glyph-atlas uploads are submitted to the host's queue, with their own fences, from inside `VulkanRender`; the host must not use that queue from another thread meanwhile | Copies cannot be recorded inside the host's render pass, and the backend's API has no earlier call. Queue order puts the upload before the frame, and barriers in the upload order it against earlier frames still sampling the atlas |
| 110 | Vulkan rotates its per-frame buffers once per frame (signalled by `EndFrame`) and destroys replaced objects `FramesInFlight` frames later, assuming the usual contract that the host waited for frame N − `FramesInFlight` before recording frame N; `VulkanReleaseTexture` was added for views destroyed sooner | Counting `Render` calls would break hosts that draw a frame twice. The contract is the one every Vulkan renderer uses. Vulkan may reuse a destroyed view's handle at once, which a cache keyed by handles cannot tell apart otherwise |
| 111 | The renderer tests run the Vulkan backend twice, with a render pass (`VulkanRenderPass`) and with dynamic rendering (`Vulkan`), with the Khronos validation layer when installed. The Windows CI job installs the Vulkan SDK and runtime but has no Vulkan driver, so the Vulkan tests skip there; Linux runs them on lavapipe | Both pipeline modes are public API. A test executable that cannot load `vulkan-1.dll` would not start at all |
| 112 | The OpenGL backend resolves its functions through the host's `GetProcAddress` into a private table and ships no loader; it declares the GL types and constants it needs itself | Carbon is a static library: sharing a loader's global function pointers works only if host and Carbon link the very same loader, and a GL header in a backend header would clash with the host's loader |
| 113 | The OpenGL backend stores primitives in an `RGBA32UI` texture buffer and reads floats with `uintBitsToFloat` | A float texture would carry the integer `Kind` as a denormal, which drivers may flush to zero; integer texels return every bit |
| 114 | `OpenGLRender` saves and restores every piece of state it changes, and draws into whatever framebuffer is bound; `OpenGLInitInfo` has only the `GetProcAddress` function and a color format, whose sRGB-ness decides `GL_FRAMEBUFFER_SRGB` | OpenGL hosts share one global state machine with Carbon; a library that leaves state behind breaks the host in ways that are hard to trace. The format of the default framebuffer cannot be queried reliably across platforms |
| 115 | The OpenGL tests create their context in a hidden GLFW window and render into a framebuffer object; the harness changes host state before every `OpenGLRender` and fails when it is not restored. CI runs them on Linux under Xvfb with llvmpipe | One harness covers rendering, debug output and state restoration. GitHub's Windows runners have no OpenGL 3.3 driver, so the tests skip there |
| 116 | `VulkanMinimalIntegration` and `OpenGLMinimalIntegration` have no documentation screenshots in `Docs/Images/Screenshots.txt`; CI uploads their screenshots as artifacts from Linux | The documentation images are rendered by the Windows CI job, which has no Vulkan or OpenGL 3.3 device. Their output matches the WebGPU example's, whose image the README shows |
| 117 | `Tests/Package` installs a renderer backend of its own, written against the installed `RendererBackend.h` only, and checks that Carbon hands it the atlas and the frames | It proves the claim that a backend can be written outside the repository, with the same mechanism as the custom-component check |
| 118 | Besides the typed `<Name>GetTextureID` functions, a host may draw a raw native handle turned into a `TextureID` with `MakeTextureID` (pointer or integer), without registering it, like Dear ImGui's `ImTextureID`. Core tracks every texture a frame draws; backends resolve an unseen ID in `Render` with default settings (Vulkan: shader-read-only layout; WebGPU: the reference is taken then) and release it after a frame unused. Registration and raw handles give the same ID. `RendererBackendVersion` stays 1 | It is the shortest path from a texture to the screen and the one ImGui users expect. Unlike ImGui, Carbon still creates and frees Vulkan descriptor sets and WebGPU bind groups itself, so the raw path adds no bookkeeping for the host. A backend written for version 1 stays correct: it skips IDs it does not know, which is what it did before |
| 119 | OpenGL ES 3.0 (and WebGL 2) is a backend of its own (`OpenGLES*` API, `CARBON_BACKEND_OPENGLES`) that shares the OpenGL backend's renderer and shaders in an ES mode. Both now store primitives in an `RGBA32UI` 2D texture instead of a texture buffer, and the shaders get their `#version` line from the renderer | A host on Android or in a browser should not need desktop names, and one renderer keeps the two from drifting apart. OpenGL ES 3.0 and WebGL 2 have no texture buffers, and 2048 texels is the widest texture OpenGL ES 3.0 guarantees, so 1024 primitives per row |
| 120 | Web browsers are a build target (Emscripten), not a backend: they render through WebGL 2 with the OpenGL ES backend, the examples use Emscripten's GLFW 3.4 port instead of the submodule, and the tests run in Node without the GPU tests. WebGPU in the browser (Dawn's emdawnwebgpu) is left for later | WebGL 2 is OpenGL ES 3.0, so no new renderer is needed and every browser that runs WebGL 2 works. The GLFW port maps the canvas to a window with Hi-DPI support; the submodule cannot be built for the web. Node has no canvas to create a context on |
| 121 | Direct3D 11 is a backend for feature level 10.0 and later, with shaders compiled by `fxc` at build time (shader model 4.0) and a public header that only forward-declares the D3D interfaces. Primitives go into a `Buffer<uint4>`. `DX11Render` takes an optional context so that deferred contexts work. The option defaults to `ON` on Windows when `fxc` is found; CI requires it on Windows, where WARP runs its tests | `fxc` ships with every Windows SDK, so the build needs nothing else; DXC does not compile shader model 4. Feature level 10.0 covers every Direct3D 11 device, and typed buffers exist there while structured buffers need 11.0. A host's `windows.h` settings (`NOMINMAX`, `WIN32_LEAN_AND_MEAN`) are not overridden by a Carbon header. Unlike OpenGL, WARP gives the Windows runners a real device |
| 122 | Direct3D 9 is a backend for shader model 3.0 with 32-bit indices. Primitives are copied into the vertices on the CPU; everything Carbon creates in `D3DPOOL_DEFAULT` is released by `DX9InvalidateDeviceObjects`, which the host calls before `Reset`, and created again on demand; state is saved with one `D3DSBT_ALL` state block that is captured again every frame | Shader model 3.0 has no integer vertex attributes and no buffer a pixel shader can index; a float texture of primitives would need a second sampler and loses the integer kind's exactness. The managed pool does not exist on Direct3D 9Ex, so the default pool with explicit invalidation (as Dear ImGui does) works on both device kinds. A state block covers the fixed-function state a Direct3D 9 host may rely on, which a hand-written list would miss; creating it once avoids a driver allocation per frame. Shader model 2.0 hardware is left out: its instruction limit cannot hold the squircle function |
| 123 | Every example is built once per backend, as `<Backend><Example>` (`WebGPUGallery` ... `DX9CustomTitleBar`; the minimal integrations are `<Backend>Minimal`, renamed from `<Backend>MinimalIntegration`). The examples other than the minimal ones are written once against `Example::App` and `Example::Host`, which drive an `Example::GraphicsDevice`; `Examples/Common/Devices` has one device per backend, and each `CarbonExample<Backend>` library links one. Host textures go through `MakeTextureID` of the native handle. In a browser only the OpenGL ES builds exist, without CustomTitleBar. Reflection is built once, on the first backend. This replaces the WebGPU-only host of decision 107 | Each backend is exercised by the full Gallery, not only by a small integration, and a user can run the same interface on any backend side by side. A build-time choice keeps every executable as small and as plain as a real application, which links one graphics API; a `--backend` switch would put all APIs into every binary. The documentation images keep coming from the WebGPU builds |
| 124 | Performance is measured by `CarbonBenchmarks` (Google Benchmark, headless contexts, steady-state frames) and `Scripts/BuildMetrics.py`, and recorded per version in `Docs/Optimizations.md`. An optimization is kept only if its metric improves by 10 % or allocations drop to zero, the tests pass and the documentation images do not change. The renderer tests' harnesses time the backend's render call for the benchmarks. CI builds the benchmarks and does not run them | Optimizing on evidence needs a repeatable measurement and a rule for what stays; the survey's hot spots were hypotheses until the baseline confirmed them. Shared CI runners are too noisy for timings |
| 125 | Rows of Sidebar, List, Table, OutlineView and ColumnView that are scrolled out of view take their space and keep their part in the selection and the keyboard, but are not hit-tested, hashed, shaped or drawn; an item out of view reports no interaction and, in List and ColumnView, has no item ID. `ClipTableRows`, `ClipListItems` and `ClipColumnViewItems` (returning a `RowRange`) let an application submit only the rows in view: the component reserves the space of the others in one piece before and one after. The range also holds the row the keyboard is moving to, and the selection is passed as an index so that it is known while its row is not submitted. OutlineView has no such function | A frame was linear in the number of rows (1.1 µs each): 112 ms for 100,000. Skipping the work of invisible rows brings the plain loop to 7 ms without any change in applications; only the application can skip the loop itself, which the ranges allow (59 µs for any number of rows). A contiguous range needs no iterator protocol: the rows between the view and a keyboard target are cheap. An outline's rows depend on which items are expanded, which only the application's traversal knows |

HIG sources read for this plan (macOS guidance): Typography, Color, Dark Mode, Layout, Motion, Accessibility,
Designing for macOS, Buttons, Toggles, Sliders, Text fields, Sidebars, Tab views, Segmented controls, Menus,
Context menus, Pop-up buttons, Pull-down buttons, Popovers, Alerts, Sheets, Steppers, Progress indicators,
Search fields, Lists and tables, Split views, Color wells, Scroll views, Offering help, Charting data, Focus and
selection, Keyboards. The Color page lists semantic names but no macOS RGB values, and the Layout page has no
macOS spacing numbers, so palette and metric values come from knowledge of macOS 11–26 and are tuned against
screenshots.
