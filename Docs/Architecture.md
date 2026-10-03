# Carbon Architecture

Status: **approved plan** (2026-10-03); implementation follows the milestones in section 15. This document is the
design contract for Carbon v1. Sections marked *Decision* record choices made where the brief left room.

Carbon is an immediate-mode C++20 UI framework that looks like macOS 11–26 (flat, no translucency) and renders
through WebGPU (Dawn) into a render pass owned by the host. It ships as two static libraries: `Carbon` (core) and
`CarbonExtensions` (components built only on Carbon's public extension API).

## 1. Principles

1. **The host owns the platform.** Carbon never creates windows, surfaces, devices or OS hooks. The host forwards
   input and hands Carbon a render pass.
2. **Immediate mode, small retained state.** The UI is rebuilt from code every frame. Carbon keeps only per-ID state:
   animations, focus, scroll offsets, measured sizes, text-edit state.
3. **Logical points everywhere.** All public coordinates and sizes are in points. Pixels exist only in the renderer
   and the glyph rasterizer.
4. **GPU-free above the renderer.** Everything except `Renderer/` compiles and runs without a GPU, so layout,
   widgets and draw-list output are unit-testable headless.
5. **Extensions prove the API.** `CarbonExtensions` may include only public Carbon headers. If an extension
   component needs something, it becomes public, documented API.

## 2. Modules

```
Framework/src/Carbon/
├── Carbon.h              umbrella header for applications
├── Extension.h           umbrella header for component authors (extension API)
├── Core/                 Platform, Log, Assert, Math (Vec2, Rect, Color, EdgeInsets), ID, Context, StateStorage
├── Input/                IO, Key, MouseButton, InputEvent, Cursor
├── Draw/                 DrawList, DrawCommand, DrawVertex, DrawPrimitive, Squircle (CPU shape function)
├── Text/                 UTF8, FontLibrary, Font, TextShaper, GlyphAtlas, TextLayout, Icons (generated)
├── Layout/               Stack, Spacer, Size, ScrollView, layout cursor
├── Animation/            Spring, Easing, AnimationSpec, Animator (per-ID state)
├── Style/                Theme, StyleColor, StyleVar, style stacks, TextStyle ramp
├── Interaction/          hit testing, ButtonBehavior, DragBehavior, focus and keyboard navigation, overlays
├── Widgets/              Text, Button, Toggle, Slider, TextField, Image, Separator, Tooltip
├── Renderer/             the only folder that calls Dawn: Renderer, pipeline, WGSL, buffers, textures
└── Assets/               declarations of the embedded fonts and shaders (bytes generated into the build tree)
```

Dependency direction (each layer uses only the ones above it):

```
Core → Input → Draw → Text → Animation → Style → Layout → Interaction → Widgets
                                                                          ↑
Renderer consumes DrawList output and GlyphAtlas pixels; nothing above depends on it.
```

**Public vs. internal headers.** Public headers are listed explicitly in `Framework/CMakeLists.txt` (a
`FILE_SET HEADERS`); only they are installed. Internal headers end in `Internal.h` or live in a `Internal/`
subfolder and use `namespace Carbon::Internal`. Three checks keep the boundary honest:

- a script (`Scripts/CheckPublicIncludes`) run by CTest and CI fails if `Extensions/` or
  `Examples/CustomComponent` include a non-public header, or if a public header includes an internal one;
- CI builds `CarbonExtensions` and `Examples/CustomComponent` against the *installed* package, where internal
  headers do not exist;
- only `Renderer/` and three public signatures (below) mention `wgpu::` types.

## 3. Public API sketch (application side)

```cpp
#include <Carbon/Carbon.h>

// ---- Setup ----
Carbon::ContextDescription description;
description.Device = device;                                    // wgpu::Device; null = headless (tests)
description.ColorFormat = surfaceFormat;                        // must match the pass Carbon renders into
description.DepthStencilFormat = wgpu::TextureFormat::Undefined; // set if the host pass has depth/stencil
description.SampleCount = 1;                                    // set if the host pass is multisampled
description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message) {};
description.Callbacks.GetClipboardText = /* std::string() */;
description.Callbacks.SetClipboardText = /* void(std::string_view) */;
description.Callbacks.SetCursor = /* void(Carbon::Cursor) */;
Carbon::Context* context = Carbon::CreateContext(description);  // also makes it current
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

Carbon::Render(pass);                                           // wgpu::RenderPassEncoder, host-owned
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
void Image(TextureID texture, Vec2 size, const ImageOptions& options = {});
void Image(const wgpu::TextureView& view, Vec2 size, const ImageOptions& options = {}); // declared in Renderer/
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
Render(pass)                 Renderer uploads atlas changes + vertex/index/primitive buffers, records draws
```

- **Single current context** (`g_Context`), as in Dear ImGui. All free functions operate on it.
- **Input queue.** `Add*Event` appends to a queue; `NewFrame` applies it. A press and release of the same button
  arriving in one frame are spread over two frames so clicks are never lost at low frame rates.
- **Hit-test stability.** Hover uses the current frame's rects but overlay occlusion uses the previous frame's
  overlay rects, so a widget under a popover never reacts on the frame the popover is submitted after it.
- **No steady-state allocations.** Draw buffers, the event queue and per-frame scratch memory keep their capacity;
  shaped text runs live in an LRU cache; per-ID state lives in pooled hash maps.

## 5. Draw list and renderer

### Draw list (GPU-free)

```cpp
struct DrawVertex { Vec2 Position; Vec2 Local; Vec2 UV; uint32_t Color; uint32_t Primitive; };   // 32 bytes
struct DrawPrimitive { Vec2 HalfSize; float Radius; float Smoothing; float StrokeWidth; float Softness; uint32_t Kind; uint32_t Pad; };
struct DrawCommand { Rect ClipRect; TextureID Texture; uint32_t IndexOffset; uint32_t IndexCount; };

class DrawList
{
public:
    void PushClipRect(const Rect& rect, bool intersectWithCurrent = true);
    void PopClipRect();

    void AddRect(const Rect& rect, Color color);
    void AddSquircle(const Rect& rect, Color color, float radius, float smoothing = DefaultSmoothing);
    void AddSquircleStroke(const Rect& rect, Color color, float radius, float width, float smoothing = DefaultSmoothing);
    void AddFocusRing(const Rect& rect, Color color, float radius, float width, float offset, float smoothing = DefaultSmoothing);
    void AddCircle(Vec2 center, float radius, Color color);
    void AddCircleStroke(Vec2 center, float radius, Color color, float width);
    void AddLine(Vec2 from, Vec2 to, Color color, float width);
    void AddShadow(const Rect& rect, Color color, float radius, float blur, Vec2 offset);
    void AddText(Vec2 position, std::string_view text, const TextSpec& spec, Color color);
    void AddImage(TextureID texture, const Rect& rect, const Rect& uv, Color tint, float radius = 0.0f);
};
```

Every shape is one quad (4 vertices, 6 indices) slightly larger than the shape; `Local` carries the position
relative to the shape centre and `Primitive` indexes a per-frame primitive array. The draw list is in points; clip
rects are in points and converted to pixel scissor rects by the renderer. Consecutive primitives that share a clip
rect and texture merge into one `DrawCommand`. Shapes and glyphs both bind the glyph atlas, so a typical frame is a
handful of draw calls; only images break batches.

The context owns one draw list per **layer** — `Background`, `Content`, `Overlay`, `Tooltip` — concatenated in
that order at `EndFrame`. That is the whole overlay mechanism on the drawing side.

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
  antialiasing, strokes and focus rings need.

Strokes are the difference of two squircles (outer shape minus the inset shape with `radius - width`), and focus
rings are a stroke on the outset shape, so ring corners stay concentric with the control.

### Renderer (Dawn)

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
- WGSL lives in `Renderer/Shaders/*.wgsl` and is embedded at build time.
- `Render(pass)` sets its own viewport, scissor, pipeline and bind groups and does not restore the host's state;
  this is documented in `Docs/Integration.md`.

## 6. Text, fonts and icons

- **Shaping.** HarfBuzz shapes UTF-8 runs (kerning and ligatures on). Runs are split by font fallback: requested
  font → Public Sans → Phosphor (icons live in the Private Use Area) → fonts added by the host, in order.
- **Variable weight.** One FreeType face per font file; a weight instance (`wght` axis) per used weight, each with
  its own HarfBuzz font. `FontWeight` is a numeric 100–900 enum.
- **Glyph atlas.** A single-channel atlas with skyline packing, rasterized at `size × contentScale` pixels. Glyph
  key: font, weight, glyph index, pixel size, horizontal sub-pixel bin (4 bins). Text origins snap to whole pixels
  vertically. The atlas grows by doubling (to a 4096² cap); when the content scale changes or the cap is hit it is
  cleared and refilled lazily. Only dirty regions are uploaded.
- **Shaped-run cache.** Keyed by hash of (text, font, weight, size); LRU, so steady-state frames do no shaping.
- **Type ramp** (`TextStyle`), from the HIG macOS table: Large Title 26/32, Title 1 22/26, Title 2 17/22,
  Title 3 15/20, Headline 13/16 bold, Body 13/16, Callout 12/15, Subheadline 11/14, Footnote 10/13,
  Caption 1 10/13, Caption 2 10/13 medium. `TextOptions::Emphasized` selects the HIG's emphasized weight.
  Public Sans is compared against SF Pro in M2 (x-height and advance widths); any size or weight adjustment is
  recorded in `Docs/Styling.md`.
- **Icons.** Phosphor regular, bold and fill fonts are embedded. `Carbon::Icons::House` etc. are
  `inline constexpr const char*` UTF-8 strings generated at build time from Phosphor's `selection.json` into the
  build tree, so icons can be drawn with `Icon()` or embedded in any label. Icon weight follows text weight
  (semibold and above use the bold font); `IconOptions::Variant = Fill` selects the filled set.

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
- **Stack identity.** Stacks take their ID from their call order inside the parent (or `options.ID` when given).
  They do **not** push onto the ID stack, so adding or removing a stack never changes widget IDs and widget
  state (focus, animation) survives layout refactors. *Decision.*
- **ScrollView** is a container with a clip rect and a persisted, spring-animated scroll offset. Overlay scroll
  thumbs (squircle pills) fade in while scrolling or hovering the track and fade out after ~1 s. It takes an
  explicit ID and pushes it on the ID stack.

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
- **Focus scopes.** Overlays open a focus scope; modal scopes (alert, sheet) trap Tab and restore the previous
  focus when they close.

## 11. Overlays

Popovers, menus, alerts, sheets and tooltips are not windows. They are submitted in the normal frame, drawn into
the `Overlay`/`Tooltip` layers and positioned against an anchor rect, flipped and clamped to stay on the display.

```cpp
void OpenOverlay(ID id);   void CloseOverlay(ID id);   bool IsOverlayOpen(ID id);
bool BeginOverlay(ID id, const OverlayOptions& options);   // .Anchor, .Placement, .Modal, .Scrim, .DismissOnOutsideClick
void EndOverlay();
```

An open overlay captures input: widgets below ignore hover and clicks inside it, modal overlays block everything
below, and a click outside a non-modal overlay dismisses it (and is swallowed). The core provides this mechanism;
the concrete components live in `CarbonExtensions`.

## 12. Extension API (`#include <Carbon/Extension.h>`)

The supported surface for custom components — what `imgui_internal.h` is for Dear ImGui, but stable and
documented.

```cpp
// Identity and per-ID state
ID GetID(std::string_view label);
template <typename T> T* GetState(ID id);                    // zero-initialized, trivially copyable T, persisted while used

// Layout
Rect AllocateItem(Vec2 size, const ItemOptions& options = {});   // reserves space in the current stack
Rect GetAvailableRect();

// Interaction
bool RegisterItem(ID id, const Rect& rect, ItemFlags flags = ItemFlags::None);   // false when clipped
Interaction ButtonBehavior(ID id, const Rect& rect, const ButtonBehaviorOptions& options = {});
// Interaction { Hovered, Pressed, Clicked, DoubleClicked, Focused, FocusVisible, Activated }
DragInteraction DragBehavior(ID id, const Rect& rect);           // { Active, Started, Ended, Delta, Total }
bool IsItemHovered();  bool IsKeyPressed(Key key, bool repeat = true);  bool IsDisabled();

// Focus
void RegisterFocusable(ID id, const Rect& rect, const FocusOptions& options = {});
bool IsFocused(ID id);  void SetFocus(ID id);  void ClearFocus();

// Drawing and text
DrawList& GetDrawList(DrawLayer layer = DrawLayer::Current);
Vec2 MeasureText(std::string_view text, const TextSpec& spec);
TextSpec GetTextSpec(TextStyle style);

// Style (applies the precedence rules)
Color GetStyleColor(StyleColor color);   float GetStyleVar(StyleVar var);
Color Resolve(const std::optional<Color>& perCall, StyleColor fallback);
float Resolve(const std::optional<float>& perCall, StyleVar fallback);

// Animation: Animate(...) from section 8.  Overlays: section 11.
```

`Examples/CustomComponent` builds a star-rating control with exactly this API, and `Docs/CustomComponents.md`
walks through it.

## 13. Logging and asserts

- `Callbacks.Log(LogLevel, source, message)`; with no callback, logs are dropped. Messages use `std::format`.
- `CB_ASSERT(condition, "format", args...)` formats, logs at `LogLevel::Fatal`, calls the optional
  `Callbacks.AssertFailed` hook (tests use it to observe asserts), then breaks into the debugger
  (`__debugbreak` on MSVC, `__builtin_trap` on GCC/Clang). `CB_VERIFY` stays active in release builds.

## 14. Build, packaging and repository

- Root `CMakeLists.txt`: options and `add_subdirectory` only. Targets `Carbon` (`Carbon::Carbon`) and
  `CarbonExtensions` (`Carbon::Extensions`), both static.
- `CARBON_DEPS_<NAME>_BUILD` / `CARBON_DEPS_<NAME>_NAME` for FreeType, HarfBuzz, GoogleTest, GLFW and stb; Dawn
  is always found (`find_package(Dawn CONFIG)` unless `CARBON_DEPS_DAWN_NAME` already exists).
- Fonts and shaders are converted to `.cpp` byte arrays at build time by `Framework/CMake/EmbedAsset.cmake`
  (pure CMake, no Python), written to the build tree and never committed. `Icons.h` is generated the same way.
- Submodules pinned to release tags: FreeType, HarfBuzz, GoogleTest (v1.18.0), GLFW (3.5.1),
  Public Sans (v2.001), Phosphor web (v2.1.2); stb has no tags and is pinned to a commit.
- **Dawn**: developed against commit `91158020c0b1cb0ddb4dc1c2c29e5a4669374f0b` (2026-09-03). That install has
  no `webgpu_glfw` helper, so the examples create their surface per platform (Win32, X11, Wayland) in
  `Examples/Common/`.
- Examples accept `--screenshot <file.png>`, `--theme light|dark` and `--scale <factor>`; screenshot mode renders
  one settled frame to an offscreen texture and never opens a window.
- CI: GitHub Actions on Windows (MSVC) and Linux (GCC, Clang); Dawn is built once per pinned commit and cached.

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

## 16. Decision log

| # | Decision | Reason |
| --- | --- | --- |
| 1 | Per-call structs are `<Widget>Options` | `TextStyle` is the type-ramp enum in the brief's own example |
| 2 | Squircle = superellipse corner patch, apex-matched to the circular arc | Analytic in the fragment shader, exact circle at smoothing 0, zero curvature at the joins |
| 3 | Stacks are identified by call order and do not push IDs | Widget state must survive layout changes |
| 4 | `ContextDescription` carries `DepthStencilFormat` and `SampleCount` | The pipeline must match the host's pass |
| 5 | Draw list stores an opaque `TextureID`; `wgpu::` types appear only in `ContextDescription`, `Render` and the `Image` overload | Keeps everything above `Renderer/` GPU-free |
| 6 | Gamma-space blending on `Unorm` targets, linearized on `UnormSrgb` | Matches the look of macOS UI on the common surface formats |
| 7 | Carbon behaves as if Full Keyboard Access is on; focus ring only after keyboard focus (always for text fields) | The brief requires full keyboard operability; ring behaviour follows macOS |
| 8 | Shortcut modifier is Ctrl by default (configurable) | Targets are Windows and Linux |
| 9 | Overlays use opaque surfaces, hairline border and an analytic shadow | Separation on pure black without translucency or blur |
| 10 | Public/internal boundary enforced by an include check and by building extensions against the installed package | The source tree has a single include root |

HIG sources read for this plan (macOS guidance): Typography, Color, Dark Mode, Layout, Motion, Accessibility,
Designing for macOS, Buttons, Toggles, Sliders, Text fields, Sidebars, Tab views, Segmented controls, Menus,
Context menus, Pop-up buttons, Pull-down buttons, Popovers, Alerts, Sheets, Steppers, Progress indicators,
Search fields, Lists and tables, Split views, Color wells, Scroll views, Offering help, Charting data, Focus and
selection, Keyboards. The Color page lists semantic names but no macOS RGB values, and the Layout page has no
macOS spacing numbers, so palette and metric values come from knowledge of macOS 11–26 and are tuned against
screenshots.
