# Renderer backends

Carbon builds the interface into graphics-API-free draw data. A *renderer backend* draws that data with one
graphics API, into a target the host owns. The host picks a backend at run time from those compiled into the
library and can switch by shutting one down and initializing another.

| Backend | CMake option | Header | Dependency |
| --- | --- | --- | --- |
| WebGPU (Dawn) | `CARBON_BACKEND_WEBGPU` | `Carbon/Backends/WebGPU/WebGPUBackend.h` | An installed [Dawn](Building.md#installing-dawn) |

A context has at most one backend. Without one it is *headless*: it builds the same draw data
(`Carbon::GetDrawData()`) but cannot render, which is what the unit tests use.

## Choosing a backend

**At build time**, `CARBON_BACKEND_<NAME>` compiles a backend into the `Carbon` library. Left unset, a backend is
built when its dependency is found; set to `ON`, a missing dependency stops the configuration with an explanation.
CMake prints the result:

```text
-- Carbon: renderer backends: WebGPU
```

Code that links `Carbon::Carbon` sees `CARBON_HAS_BACKEND_<NAME>` defined for every backend that was built, and an
installed package lists them in `Carbon_BACKENDS` after `find_package(Carbon)`. See [Building](Building.md).

**At run time**, the host creates a context and initializes one backend with the native objects of its API:

```cpp
Carbon::Context* context = Carbon::CreateContext(description);
Carbon::WebGPUInit(info);          // or another backend's Init function
// ... frames ...
Carbon::WebGPUShutdown();
Carbon::DestroyContext(context);   // also shuts down a backend that is still installed
```

Every backend has the same shape, in `namespace Carbon`:

| Function | Purpose |
| --- | --- |
| `<Name>Init(const <Name>InitInfo&)` | Installs the backend into the current context. Returns `false` and logs why on failure |
| `<Name>Shutdown()` | Removes it and releases what it created |
| `<Name>Render(...)` | Draws the last finished frame into the host's target, after `EndFrame` |
| `<Name>GetTextureID(...)` | Returns the `TextureID` of a host texture, for `Image` and `DrawList::AddImage` |
| `<Name>Image(...)` | `Image(<Name>GetTextureID(...), size, options)` in one call |

To switch backends, call the old one's `Shutdown`, then the new one's `Init`. Carbon then uploads the glyph atlas
again; everything else (layout, animation, focus, text caches) stays.

## What every backend does the same way

- **Colors.** Carbon's colors are sRGB. With an `…Unorm` target they are written as they are, so blending happens
  in gamma space, which is what macOS interfaces look like. With an `…UnormSrgb` target Carbon converts them to
  linear values and the GPU blends in linear space; translucent edges then look slightly lighter. Prefer a
  `…Unorm` format for an interface.
- **Blending.** Premultiplied alpha, on top of whatever the target already holds. Carbon neither tests nor writes
  depth.
- **Size.** The target is `display size × content scale` pixels; Carbon sets the viewport to that.
- **Host textures.** `<Name>GetTextureID` registers a texture for the current frame. Carbon keeps what it needs to
  draw the texture while the texture is registered or drawn, and releases it once a whole frame passes without
  either. Call it every frame, or keep drawing the ID you got, and keep the texture alive until then. Textures are
  sampled with linear filtering and treated as straight (not premultiplied) alpha.
- **Rendering twice.** `<Name>Render` may be called more than once per frame, for several targets.

## WebGPU

The WebGPU backend runs on [Dawn](https://dawn.googlesource.com/dawn) and records into a render pass the host
begins and ends. [Examples/WebGPUMinimalIntegration](../Examples/WebGPUMinimalIntegration/Main.cpp) is a complete
host with GLFW.

```cpp
#include <Carbon/Backends/WebGPU/WebGPUBackend.h>
#include <Carbon/Carbon.h>

Carbon::Context* context = Carbon::CreateContext(description);

Carbon::WebGPUInitInfo info;
info.Device = device;                                        // wgpu::Device
info.ColorFormat = Carbon::TextureFormat::BGRA8Unorm;        // the format of the passes Carbon draws into
info.DepthStencilFormat = Carbon::TextureFormat::Undefined;  // set when the passes have a depth-stencil attachment
info.SampleCount = 1;                                        // set when the passes are multisampled
if (!Carbon::WebGPUInit(info))
    return;

// Every frame, after EndFrame:
wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);   // your pass
DrawScene(pass);                                                           // your own content, if any
Carbon::WebGPURender(pass);                                                // Carbon's interface on top
pass.End();

// A texture of yours inside the interface:
Carbon::WebGPUImage(sceneView, Carbon::Vec2(320, 180), { .CornerRadius = 10.0f });   // wgpu::TextureView

Carbon::WebGPUShutdown();
Carbon::DestroyContext(context);
```

- **Formats.** `ColorFormat`, `DepthStencilFormat` and `SampleCount` must match every pass given to
  `WebGPURender`; Carbon's pipeline is built for them. `Depth24Unorm` and `Depth24UnormStencil8` map to WebGPU's
  `Depth24Plus` formats.
- **State.** `WebGPURender` uploads with the device's queue (`WriteBuffer`, `WriteTexture`), sets its own
  pipeline, bind groups, vertex and index buffers, viewport and scissor rectangle, and does not restore the
  previous ones. Draw your content first, or set your state again afterwards.
- **Textures.** `WebGPUGetTextureID(view)` keeps a reference to the `wgpu::TextureView` while it is in use.
- **Windows: d3dcompiler_47.dll.** Dawn's Direct3D backends compile shaders with `d3dcompiler_47.dll` and, unless
  Dawn was built with `DAWN_FORCE_SYSTEM_COMPONENT_LOAD=ON`, only look for it next to the executable. Ship the DLL
  from the Windows SDK (`Redist/D3D/x64`) with your application. Carbon's examples and tests copy it with
  `carbon_copy_dawn_runtime(<target>)`, which the installed package provides too; without it, device creation fails
  with `DynamicLib.Open: d3dcompiler_47.dll`.

## Writing your own backend

A backend for another API (Direct3D 12, OpenGL ES, Metal, an engine's own rendering layer) is written against one
public header, `Carbon/Renderer/RendererBackend.h`, without changing Carbon. The backends in
`Framework/src/Carbon/Backends/` use nothing else and are the best examples.

### The interface

```cpp
static_assert(Carbon::RendererBackendVersion == 1);   // the contract this backend was written for

class MyBackend : public Carbon::RendererBackend
{
public:
    ~MyBackend() override;                                        // release everything: this is the shutdown
    std::string_view GetName() const override { return "Mine"; }
    Carbon::RendererBackendCapabilities GetCapabilities() const override;   // optional: device limits
    void BeginFrame(uint64_t frameCount) override;                // optional: no GPU work
    void EndFrame() override;                                     // optional: no GPU work
    void UpdateGlyphAtlas(const Carbon::GlyphAtlasUpdate& update) override;
    void Render(const Carbon::DrawData& drawData) override;
    void ReleaseTexture(Carbon::TextureID texture) override;
};
```

Native objects never pass through this interface. Give the backend functions of its own, in the same shape as the
built-in ones, and keep what they receive in the backend object:

```cpp
bool MyInit(const MyInitInfo& info)
{
    return Carbon::InstallRendererBackend(std::make_unique<MyBackend>(info));   // the context owns it
}

void MyShutdown()
{
    if (Carbon::GetRendererBackend<MyBackend>() != nullptr)
        Carbon::RemoveRendererBackend();
}

void MyRender(MyCommandList& commands)
{
    MyBackend* backend = Carbon::GetRendererBackend<MyBackend>();   // null unless MyBackend is installed
    if (backend == nullptr)
        return;
    backend->SetTarget(&commands);     // what Render will record into
    Carbon::RenderDrawData();          // calls ReleaseTexture, UpdateGlyphAtlas and Render as needed
    backend->SetTarget(nullptr);
}

Carbon::TextureID MyGetTextureID(MyTexture* texture)
{
    MyBackend* backend = Carbon::GetRendererBackend<MyBackend>();
    if (backend == nullptr)
        return {};
    backend->Remember(texture);                                              // keep what binding it needs
    return Carbon::RegisterHostTexture(reinterpret_cast<uintptr_t>(texture));   // the key becomes the TextureID
}
```

`GetRendererBackend<T>()` identifies the type safely without RTTI, so a backend's functions can never pick up
another backend's object.

### When Carbon calls what

| Call | From | What to do |
| --- | --- | --- |
| `BeginFrame(frameCount)` | `NewFrame` | Bookkeeping only, such as counting down to destroying objects that frames in flight may still use |
| `EndFrame()` | `EndFrame` | Bookkeeping only. The draw data is final; every `Render` until the next `EndFrame` gets the same data, so upload it once per frame |
| `UpdateGlyphAtlas(update)` | `RenderDrawData`, `FlushGlyphAtlas` | Copy atlas rows into the atlas texture (below) |
| `Render(drawData)` | `RenderDrawData` | Draw the frame into the target your render function stored |
| `ReleaseTexture(id)` | `RenderDrawData`, `FlushGlyphAtlas`, `ReleaseHostTexture`, removal | Drop what you keep for that host texture |
| destructor | `<Name>Shutdown`, `RemoveRendererBackend`, `DestroyContext` | Release everything; the backend's context is current |

The GPU is touched only in calls your own functions make (`RenderDrawData`, `FlushGlyphAtlas`,
`ReleaseHostTexture`) and in the destructor, never from `NewFrame` or `EndFrame`. So a backend may require a
current OpenGL context, or the render thread, in its own functions, and nowhere else.

### Drawing the frame

`DrawData` holds spans of vertices, 32-bit indices, primitives and commands, plus the display size in points and
the content scale. Upload them however suits the API; the WGSL shader of the WebGPU backend
(`Backends/WebGPU/Shaders/Carbon.wgsl`) is the reference implementation of everything below.

- **Vertices** (`DrawVertex`, 32 bytes): position in points, the position relative to the shape's center
  (`Local`), UV, a straight-alpha sRGB color packed as RGBA8 (R in the lowest byte), and the index of the
  vertex's primitive.
- **Primitives** (`DrawPrimitive`, 32 bytes): half size, corner radius, corner smoothing, stroke width, softness
  and a kind (`Squircle`, `SquircleStroke`, `Shadow`, `Glyph`, `Image`). The fragment shader reads the primitive
  of a vertex and evaluates the shape per pixel. Storage buffers, texture buffers, a float texture or expanding
  primitives into the vertices are all fine.
- **Position.** Points map to clip space with the display size: `x / width * 2 - 1`, `1 - y / height * 2` (y
  points down).
- **Shapes.** The squircle distance function (`SquircleDistance` in `Draw/Squircle.cpp` and the shader) gives a
  signed distance in points; coverage is `clamp(0.5 - distance * contentScale, 0, 1)`. A stroke is the outer shape
  minus the shape inset by the stroke width; a shadow is a smoothstep over `2 × softness`; a glyph takes the atlas
  coverage; an image multiplies the color by the texel and is masked by the squircle.
- **UVs.** Glyph UVs are in atlas texels (divide by the atlas size); image UVs are 0 to 1, top-left origin.
- **Colors.** Convert the vertex color to linear when the target is sRGB, then write premultiplied alpha:
  `rgb × alpha × coverage, alpha × coverage`, blended with `One, OneMinusSrcAlpha`.
- **Commands.** Draw each `DrawCommand` in order: `IndexCount` indices from `IndexOffset`, base vertex 0, with the
  clip rectangle (points, times the content scale, rounded to whole pixels and clamped to the target) as scissor.
  `TextureID()` (zero) is the glyph atlas; any other ID is a host texture. Skip a command whose texture you do not
  know, and log a warning.
- **Sampling.** Linear filtering, clamped to the edge.

### The glyph atlas

The atlas is a single-channel (8-bit coverage) texture. `UpdateGlyphAtlas` receives the whole atlas each time
(`Pixels`, `Width × Height` bytes, rows from the top), plus the rows that changed:

- `IsFull` is set for the first update after the backend is installed, after the atlas grew or was cleared (a new
  `Generation`, possibly a new size), and after `InvalidateGlyphAtlas()`. Create the texture if the size changed
  and write every row.
- Otherwise only rows `[FirstRow, FirstRow + RowCount)` changed; rows a backend skipped while it was not rendering
  are included.
- The first `Render` is always preceded by a full update, so the atlas texture exists even for a frame without text.
- If you cannot take an update (the texture could not be created), call `InvalidateGlyphAtlas()`: the next
  update is a full one again. Do the same after losing the device (a Direct3D 9 reset, a lost GL ES context).
- APIs that cannot upload textures where the frame is drawn (inside a render pass) can offer a function the host
  calls earlier, which calls `FlushGlyphAtlas()`; or they upload on a separate command buffer.
- `GetCapabilities().MaxTextureSize` keeps the atlas within the device's limit.

### Host textures

`RegisterHostTexture(key)` marks a texture as used in the current frame and returns a `TextureID` whose value is
the key; any nonzero 64-bit value that identifies the texture to your backend works (a handle, a pointer, a slot
number). Carbon also counts a texture as used when the frame's draw data draws it. After a whole frame without
either, Carbon calls `ReleaseTexture`. If an API may reuse a handle for a new texture before that (Vulkan image
views, Direct3D descriptors), offer a function that calls `ReleaseHostTexture(key)` before the host destroys the
texture.

### Checking a backend

Carbon's own tests run every built-in backend through the same suite: the renderer tests compare each pixel of a
squircle with the CPU distance function, and `BackendCompareTests` renders a fixed scene in both themes at scale 1
and 2 and compares it with the WebGPU rendering. `Tests/src/Support/BackendHarness.h` shows what a backend needs
to be tested that way.
