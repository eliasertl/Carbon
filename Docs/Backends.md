# Renderer backends

Carbon builds the interface into graphics-API-free draw data. A *renderer backend* draws that data with one
graphics API, into a target the host owns. The host picks a backend at run time from those compiled into the
library and can switch by shutting one down and initializing another.

| Backend | CMake option | Header | Dependency |
| --- | --- | --- | --- |
| WebGPU (Dawn) | `CARBON_BACKEND_WEBGPU` | `Carbon/Backends/WebGPU/WebGPUBackend.h` | An installed [Dawn](Building.md#installing-dawn) |
| Vulkan | `CARBON_BACKEND_VULKAN` | `Carbon/Backends/Vulkan/VulkanBackend.h` | The Vulkan headers and loader, and `glslc` ([Vulkan SDK](https://vulkan.lunarg.com)) |
| OpenGL 3.3 | `CARBON_BACKEND_OPENGL` | `Carbon/Backends/OpenGL/OpenGLBackend.h` | None: the host loads OpenGL |
| OpenGL ES 3.0 / WebGL 2 | `CARBON_BACKEND_OPENGLES` | `Carbon/Backends/OpenGLES/OpenGLESBackend.h` | None: the host creates the context |
| Direct3D 11 | `CARBON_BACKEND_DX11` | `Carbon/Backends/DX11/DX11Backend.h` | Windows, and `fxc` from the Windows SDK |
| Direct3D 9 | `CARBON_BACKEND_DX9` | `Carbon/Backends/DX9/DX9Backend.h` | Windows, and `fxc` from the Windows SDK |

A context has at most one backend. Without one it is *headless*: it builds the same draw data
(`Carbon::GetDrawData()`) but cannot render, which is what the unit tests use.

## Choosing a backend

**At build time**, `CARBON_BACKEND_<NAME>` compiles a backend into the `Carbon` library. Left unset, a backend is
built when its dependency is found; set to `ON`, a missing dependency stops the configuration with an explanation.
CMake prints the result:

```text
-- Carbon: renderer backends: WebGPU, Vulkan, OpenGLES, OpenGL, DX11, DX9
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
- **Host textures.** A texture of yours is drawn by its `TextureID`, which comes from one of two places:
  - `<Name>GetTextureID(...)` takes the API's own texture type, checked by the compiler, and registers it for the
    current frame.
  - `Carbon::MakeTextureID(handle)` turns a raw native handle into an ID without registering anything, as Dear
    ImGui's `ImTextureID` does: the C handle of a WebGPU texture view (`view.Get()`), a Vulkan image view, an
    OpenGL texture name. The backend resolves the handle the first time it draws it.

  Both give the same ID for the same texture, and both follow one rule: Carbon keeps what it needs to draw the
  texture while the texture is registered or drawn, and releases it once a whole frame passes without either. Keep
  the texture alive until then. Textures are sampled with linear filtering and treated as straight (not
  premultiplied) alpha.

  ```cpp
  Carbon::Image(Carbon::MakeTextureID(sceneView.Get()), Carbon::Vec2(320, 180));   // WebGPU, raw handle
  Carbon::Image(Carbon::MakeTextureID(sceneImageView), Carbon::Vec2(320, 180));    // Vulkan
  Carbon::Image(Carbon::MakeTextureID(sceneTexture), Carbon::Vec2(320, 180));      // OpenGL
  ```

  A raw handle cannot say what the typed functions can: on Vulkan it is sampled in
  `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`, and on WebGPU Carbon takes its reference to the view only when it
  draws it, so the view must be alive at `WebGPURender`. A wrong handle is not caught at compile time.
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
  `MakeTextureID(view.Get())` draws the view without registering it; Carbon takes its reference when
  `WebGPURender` first draws it.
- **Windows: d3dcompiler_47.dll.** Dawn's Direct3D backends compile shaders with `d3dcompiler_47.dll` and, unless
  Dawn was built with `DAWN_FORCE_SYSTEM_COMPONENT_LOAD=ON`, only look for it next to the executable. Ship the DLL
  from the Windows SDK (`Redist/D3D/x64`) with your application. Carbon's examples and tests copy it with
  `carbon_copy_dawn_runtime(<target>)`, which the installed package provides too; without it, device creation fails
  with `DynamicLib.Open: d3dcompiler_47.dll`.

## Vulkan

The Vulkan backend records into a command buffer, inside a render pass instance the host begins.
[Examples/VulkanMinimalIntegration](../Examples/VulkanMinimalIntegration/Main.cpp) is a complete host with GLFW,
a swapchain and a host triangle in the same render pass.

```cpp
#include <Carbon/Backends/Vulkan/VulkanBackend.h>

Carbon::VulkanInitInfo info;
info.Instance = instance;
info.PhysicalDevice = physicalDevice;
info.Device = device;
info.Queue = graphicsQueue;                              // the queue the host submits the frame to
info.QueueFamily = graphicsQueueFamily;
info.FramesInFlight = 2;                                 // how many frames the host records ahead
info.RenderPass = renderPass;                            // or VK_NULL_HANDLE for dynamic rendering, below
info.Subpass = 0;
info.ColorFormat = Carbon::TextureFormat::BGRA8Unorm;    // the color attachment's format
if (!Carbon::VulkanInit(info))
    return;

// Every frame, after EndFrame, inside the host's render pass:
vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
DrawScene(commandBuffer);                                // your own content, if any
Carbon::VulkanRender(commandBuffer);                     // Carbon's interface on top
vkCmdEndRenderPass(commandBuffer);

// A texture of yours, in the layout it has when the frame samples it:
Carbon::VulkanImage(sceneView, Carbon::Vec2(320, 180), {}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

vkDeviceWaitIdle(device);                                // the GPU must be done with Carbon's frames
Carbon::VulkanShutdown();
```

**Render pass or dynamic rendering.** With `RenderPass` set, Carbon builds its pipeline for that render pass and
`Subpass`; any compatible render pass works later (same attachment formats and sample count), so recreating a
swapchain of the same format needs nothing from Carbon. With `RenderPass` left `VK_NULL_HANDLE`, Carbon builds the
pipeline for dynamic rendering from `ColorFormat`, `DepthStencilFormat` and `SampleCount`, and the host draws
between `vkCmdBeginRendering` and `vkCmdEndRendering`. The host must then have enabled the `dynamicRendering`
feature: core in Vulkan 1.3 (`VkPhysicalDeviceVulkan13Features::dynamicRendering`), or the
`VK_KHR_dynamic_rendering` extension with its feature struct on Vulkan 1.2. Carbon itself needs nothing beyond
Vulkan 1.0 otherwise.

**Frames in flight.** Carbon keeps vertex, index and primitive buffers for each of `FramesInFlight` frames and
rotates through them once per frame, so it never writes a buffer the GPU may still read. It relies on the usual
contract: when the host records frame N, it has waited for the fence of frame N − `FramesInFlight`. Objects a
recorded frame may still use (an atlas image that was replaced, a released texture's descriptor set, a buffer
that grew) are destroyed `FramesInFlight` frames later. Calling `VulkanRender` again in the same frame, for a
second target, reuses the frame's buffers.

**Glyph-atlas uploads.** Inside a render pass Carbon cannot record copies, so atlas changes are recorded into a
command buffer of Carbon's own and submitted to `Queue` from inside `VulkanRender`, before the host submits the
frame. Barriers in it order the copy after earlier frames' sampling and before later frames'. The host must
therefore not use the queue from another thread during `VulkanRender`.

**State.** `VulkanRender` binds its own pipeline, descriptor sets, vertex and index buffers, and sets viewport,
scissor and push constants; it restores nothing. Viewport and scissor are dynamic state in Carbon's pipeline.

**Textures.** `VulkanGetTextureID(view, layout)` creates a descriptor set (combined image sampler) for the view,
in the layout the image has whenever Carbon's commands sample it, and caches it until the view goes a whole frame
unused. `MakeTextureID(view)` draws a view without registering it, in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`;
the descriptor set is then created when `VulkanRender` first draws it. A host that destroys a view sooner than a
frame after its last use calls `VulkanReleaseTexture(view)` first, because Vulkan may give a new view the same
handle. Without `DescriptorPool`, Carbon creates a pool for 1024 texture descriptor sets; a host
pool must have been created with `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`.

**Memory and objects.** Carbon allocates device memory with a small allocator of its own (a few large blocks,
no VMA), and uses `PipelineCache` and `Allocator` when the host gives them. It links the Vulkan loader
(`Vulkan::Vulkan`) and calls Vulkan functions directly.

**Building.** The backend needs the Vulkan headers and loader, and `glslc` to compile its shaders
(`Backends/Vulkan/Shaders/Carbon.vert` and `Carbon.frag`, GLSL 450) to SPIR-V at build time. The
[Vulkan SDK](https://vulkan.lunarg.com) has all of it; on Ubuntu, `libvulkan-dev` and `glslc`. See
[Building](Building.md).

## OpenGL

The OpenGL backend needs an OpenGL 3.3 core profile context (or later, or a compatibility profile of 3.3) and
draws into the framebuffer that is bound when the host calls `OpenGLRender`.
[Examples/OpenGLMinimalIntegration](../Examples/OpenGLMinimalIntegration/Main.cpp) is a complete host with GLFW
and a host triangle in the same framebuffer.

```cpp
#include <Carbon/Backends/OpenGL/OpenGLBackend.h>

// With the host's OpenGL context current:
Carbon::OpenGLInitInfo info;
info.GetProcAddress = &glfwGetProcAddress;               // or SDL_GL_GetProcAddress, eglGetProcAddress, ...
info.ColorFormat = Carbon::TextureFormat::RGBA8Unorm;    // RGBA8UnormSrgb for an sRGB framebuffer
if (!Carbon::OpenGLInit(info))
    return;

// Every frame, after EndFrame:
glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);          // the target, display size × content scale pixels
DrawScene();                                             // your own content, if any
Carbon::OpenGLRender();                                  // Carbon's interface on top
// your state is as you left it

// A texture of yours (a GLuint):
Carbon::OpenGLImage(sceneTexture, Carbon::Vec2(320, 180));

Carbon::OpenGLShutdown();                                // with the context still current
```

**The loader.** Carbon includes no OpenGL header and links no loader. `OpenGLInit` resolves the 55 functions it
uses through the host's `GetProcAddress` into a private table, and fails with the name of the first one that is
missing. The function must return OpenGL 1.0 and 1.1 functions too, which `glfwGetProcAddress`,
`SDL_GL_GetProcAddress` and `eglGetProcAddress` (EGL 1.5) do; plain `wglGetProcAddress` does not. Carbon is a
static library: sharing a loader's global function pointers with the host (glad, GLEW) works only if both link the
very same loader, so Carbon keeps its own table instead.

**The context.** The context Carbon was initialized with must be current in every `OpenGL*` call and when the
backend is shut down (`OpenGLShutdown`, `RemoveRendererBackend`, `DestroyContext`), and those calls must come
from the thread it is current on. Carbon calls OpenGL nowhere else: not from `NewFrame` or `EndFrame`.

**State.** `OpenGLRender` saves every piece of state it changes and restores it before it returns, so the host's
rendering before and after is not disturbed: the program, vertex array, array buffer, active texture, the
texture (2D and buffer) and sampler bindings of units 0 and 1, blend enable, equations and functions, scissor test
and box, culling, depth test and mask, stencil test, color mask, viewport, polygon mode, primitive restart, logic
op and `GL_FRAMEBUFFER_SRGB`, plus the pixel-unpack state while the glyph atlas is uploaded. The element buffer
binding belongs to Carbon's own vertex array. Carbon's tests check the restoration after every frame.

**Coordinates.** OpenGL's window coordinates start at the bottom left; Carbon flips its clip space and scissor
rectangles, so the output matches the other backends. Textures are sampled with row 0 at the top of the image, as
uploaded from memory; a texture the host rendered into through a framebuffer has row 0 at the bottom, so show it
with `ImageOptions::UV = Rect(0, 1, 1, -1)`.

**sRGB.** With an sRGB `ColorFormat`, Carbon writes linear values and enables `GL_FRAMEBUFFER_SRGB` while it
draws, so the GPU encodes them; otherwise it disables `GL_FRAMEBUFFER_SRGB`, so its sRGB colors are written as
they are, which matches the other backends on a `…Unorm` target. Either way the host's setting is restored.

**Textures.** `OpenGLGetTextureID(texture)` takes a texture name; `MakeTextureID(texture)` gives the same ID
without registering it, which on OpenGL makes no difference at all. Carbon samples the texture with its own sampler
object (linear, clamped to the edge), so the texture's own filter settings do not matter, and keeps nothing else
for it.

**Shaders.** `Backends/OpenGL/Shaders/Carbon.vert` and `Carbon.frag` are ports of the WGSL, embedded as text
without a `#version` line: the backend adds `#version 330 core` (or, for OpenGL ES, `#version 300 es` with default
precisions) and the driver compiles them in `OpenGLInit`; a compile error is logged with the driver's message.
Primitives reach the fragment shader through an `RGBA32UI` 2D texture, two texels each and 1024 per row, read
exactly with `texelFetch`.

## OpenGL ES

The OpenGL ES backend needs an OpenGL ES 3.0 context or later, or WebGL 2 in a browser (see
[Building](Building.md#emscripten-web-browsers)): Android, iOS through ANGLE, embedded Linux and the web. It works
like the OpenGL backend, with its own API and type, and draws into the framebuffer that is bound when the host
calls `OpenGLESRender`. [Examples/OpenGLESMinimalIntegration](../Examples/OpenGLESMinimalIntegration/Main.cpp) runs
natively with GLFW and in a browser.

```cpp
#include <Carbon/Backends/OpenGLES/OpenGLESBackend.h>

// With the host's OpenGL ES context current:
Carbon::OpenGLESInitInfo info;
info.GetProcAddress = &eglGetProcAddress;                // or glfwGetProcAddress, SDL_GL_GetProcAddress, ...
info.ColorFormat = Carbon::TextureFormat::RGBA8Unorm;
if (!Carbon::OpenGLESInit(info))
    return;

// Every frame, after EndFrame:
DrawScene();                                             // your own content, if any
Carbon::OpenGLESRender();                                // Carbon's interface on top, into the bound framebuffer

Carbon::OpenGLESShutdown();                              // with the context still current
```

It shares its renderer, its shaders and its state rules with the OpenGL backend (above); the differences are what
OpenGL ES lacks:

- The shaders are compiled as GLSL ES 3.00 with high precision. Primitives are read from an `RGBA32UI` 2D texture
  with `texelFetch`, 1024 per row, which every OpenGL ES 3.0 device and WebGL 2 support (OpenGL ES 3.0 has no
  texture buffers).
- OpenGL ES has no `GL_FRAMEBUFFER_SRGB`, polygon mode, logic op or primitive-restart switch, so Carbon leaves
  those alone. sRGB framebuffers always encode; give `ColorFormat` an sRGB format for them so that Carbon writes
  linear values.
- `GetProcAddress` must return every OpenGL ES 3.0 function. With Emscripten, link with
  `-sGL_ENABLE_GET_PROC_ADDRESS` (Carbon's examples do).

## Direct3D 11

The Direct3D 11 backend needs a device of feature level 10.0 or later and draws into the render target that is bound
to the context when the host calls `DX11Render`.
[Examples/DX11MinimalIntegration](../Examples/DX11MinimalIntegration/Main.cpp) is a complete host with GLFW, a flip
model swap chain and a host triangle in the same render target.

```cpp
#include <d3d11.h>
#include <Carbon/Backends/DX11/DX11Backend.h>

Carbon::DX11InitInfo info;
info.Device = device;                                    // Carbon holds a reference to both
info.Context = immediateContext;
info.ColorFormat = Carbon::TextureFormat::BGRA8Unorm;    // the render target view's format
if (!Carbon::DX11Init(info))
    return;

// Every frame, after EndFrame:
immediateContext->OMSetRenderTargets(1, &backBufferView, nullptr);  // display size × content scale pixels
DrawScene();                                             // your own content, if any
Carbon::DX11Render();                                    // Carbon's interface on top
swapChain->Present(1, 0);

// A texture of yours (an ID3D11ShaderResourceView*):
Carbon::DX11Image(sceneView, Carbon::Vec2(320, 180));

Carbon::DX11Shutdown();                                  // before the device is released
```

**Headers and linking.** `DX11Backend.h` only declares the three interfaces it names, so it pulls in no Windows
header; include `d3d11.h` yourself. Carbon calls only methods of the objects it is given, so it links nothing: the
host links `d3d11.lib` (and `dxgi.lib` for a swap chain) as it does anyway.

**Contexts and threads.** `DX11Render` records into the context from `DX11InitInfo`, or into the one it is given:
`DX11Render(deferredContext)` records Carbon's commands, including glyph-atlas uploads, into a deferred context
for a command list. Calls must not overlap with other use of the same context; Carbon creates objects on the device
only from `DX11Init` and from `DX11Render`, when the glyph atlas or a buffer grows.

**State.** `DX11Render` saves every piece of pipeline state it changes and restores it before it returns: input
layout, topology, vertex and index buffers, vertex, geometry and pixel shaders, the constant buffers of slot 0, the
pixel shader resources of slots 0 and 1, sampler 0, rasterizer, blend and depth-stencil states, viewports and
scissor rectangles. The render targets stay as the host bound them. Carbon's tests check the restoration after
every frame, with the debug layer on when it is installed.

**sRGB.** With an sRGB `ColorFormat` (an `…_SRGB` render target view), Carbon writes linear values and Direct3D
encodes them; otherwise it writes its sRGB colors as they are.

**Textures.** `DX11GetTextureID(view)` takes a shader resource view; `MakeTextureID(view)` gives the same ID without
registering it. Either way Carbon holds a reference to the view from the first frame that draws it until a whole
frame passes without it, so the host may release its own reference at any time. Views are sampled with linear
filtering, clamped to the edge.

**Shaders.** `Backends/DX11/Shaders/Carbon.hlsl` is a port of the WGSL to shader model 4.0. `fxc` compiles it at
build time into bytecode headers in the build tree; CMake finds `fxc` in the newest Windows SDK, or takes
`-DCARBON_FXC_EXECUTABLE=<path>`. Primitives reach the pixel shader through a `Buffer<uint4>`, two elements each,
whose floats are read back with `asfloat`. The glyph atlas is an `R8_UNORM` texture, updated row range by row range
with `UpdateSubresource`.

## Direct3D 9

The Direct3D 9 backend needs a device with vertex and pixel shader 3.0 and 32-bit indices, which every Direct3D 9
GPU of the last fifteen years has, and draws into render target 0 between the host's `BeginScene` and `EndScene`.
It is meant for applications and engines that still render with Direct3D 9; new code is better served by
Direct3D 11. [Examples/DX9MinimalIntegration](../Examples/DX9MinimalIntegration/Main.cpp) is a complete host with
GLFW, a device reset on resize and a fixed-function host triangle.

```cpp
#include <d3d9.h>
#include <Carbon/Backends/DX9/DX9Backend.h>

Carbon::DX9InitInfo info;
info.Device = device;                                    // Carbon holds a reference; a 9Ex device works too
info.ColorFormat = Carbon::TextureFormat::BGRA8Unorm;    // BGRA8UnormSrgb to write through D3DRS_SRGBWRITEENABLE
if (!Carbon::DX9Init(info))
    return;

// Every frame, after EndFrame:
device->BeginScene();
DrawScene();                                             // your own content, if any
Carbon::DX9Render();                                     // Carbon's interface on top
device->EndScene();
device->Present(nullptr, nullptr, nullptr, nullptr);

// Before resetting the device:
Carbon::DX9InvalidateDeviceObjects();                    // Carbon creates its objects again in the next DX9Render
device->Reset(&presentParameters);

// A texture of yours (an IDirect3DTexture9*):
Carbon::DX9Image(sceneTexture, Carbon::Vec2(320, 180));

Carbon::DX9Shutdown();                                   // before the device is released
```

**Device resets.** Carbon keeps its vertex and index buffers, its glyph atlas and its state block in
`D3DPOOL_DEFAULT` (the managed pool does not exist on Direct3D 9Ex devices). `DX9InvalidateDeviceObjects` releases
them and Carbon's references to host textures, as `IDirect3DDevice9::Reset` requires; the next `DX9Render` creates
them again and uploads the whole glyph atlas. While the device is lost, `DX9Render` draws nothing and the frame's
atlas changes wait. Carbon's tests run every renderer test a second time with the objects released before every
frame (`DX9Invalidate`).

**State.** `DX9Render` captures the device's state in a `D3DSBT_ALL` state block, created once and captured again
every frame, and applies it after drawing; the viewport and scissor rectangle are restored separately. Carbon draws
with shaders, so a host that uses the fixed-function pipeline gets its texture stage states back unchanged.

**No integer attributes, no buffer reads.** Shader model 3.0 cannot index a buffer from the pixel shader, so Carbon
copies each vertex's primitive (size, radius, smoothing, stroke, softness and kind) into the vertex itself while it
fills the vertex buffer: 56 bytes per vertex. Direct3D 9 puts pixel centers on integer coordinates; the vertex
shader moves everything by half a pixel, so the output matches the other backends.

**sRGB and textures.** Direct3D 9 has no sRGB surface formats: with an sRGB `ColorFormat` Carbon writes linear values
with `D3DRS_SRGBWRITEENABLE`. `DX9GetTextureID(texture)` and `MakeTextureID(texture)` take an `IDirect3DTexture9*`;
level 0 is sampled with linear filtering, clamped, without `D3DSAMP_SRGBTEXTURE`. The glyph atlas is a dynamic `L8`
texture.

**Shaders.** `Backends/DX9/Shaders/Carbon.hlsl` is a port of the WGSL to shader model 3.0, compiled by `fxc` at
build time like the Direct3D 11 backend's.

## Writing your own backend

A backend for another API (Direct3D 12, Metal, an engine's own rendering layer) is written against one
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
  `TextureID()` (zero) is the glyph atlas; any other ID is a host texture. An ID your backend has not seen is a
  native handle the host drew without registering it (`MakeTextureID`): resolve it as your `GetTextureID` function
  would with default settings, or skip the command with a warning if your API cannot.
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
number); use `MakeTextureID(handle).Value` as the key, so that a registered texture and the same texture drawn
by its raw handle are one texture. Carbon also counts a texture as used when the frame's draw data draws it, even
one that was never registered. After a whole frame without
either, Carbon calls `ReleaseTexture`. If an API may reuse a handle for a new texture before that (Vulkan image
views, Direct3D descriptors), offer a function that calls `ReleaseHostTexture(key)` before the host destroys the
texture.

### Checking a backend

Carbon's own tests run every built-in backend through the same suite (Vulkan twice, once with a render pass and
once with dynamic rendering, with the validation layer when it is installed): the renderer tests compare each pixel of a
squircle with the CPU distance function, and `BackendCompareTests` renders a fixed scene in both themes at scale 1
and 2 and compares it with the WebGPU rendering. `Tests/src/Support/BackendHarness.h` shows what a backend needs
to be tested that way.
