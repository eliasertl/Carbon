#pragma once

#include <cstdint>

#include <webgpu/webgpu_cpp.h>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Renderer/TextureFormat.h"
#include "Carbon/Widgets/Image.h"

/// The WebGPU renderer backend, built on Dawn. Available when Carbon was built with CARBON_BACKEND_WEBGPU (then
/// CARBON_HAS_BACKEND_WEBGPU is defined). See Docs/Backends.md.

namespace Carbon
{
    /// What the WebGPU backend needs from the host. The formats and the sample count must match the render
    /// passes given to WebGPURender.
    struct WebGPUInitInfo
    {
        /// The device Carbon creates its pipeline, buffers and glyph atlas on. Carbon keeps a reference to it.
        wgpu::Device Device = {};
        /// Format of the passes' color attachment. With an sRGB format Carbon writes linear values.
        TextureFormat ColorFormat = TextureFormat::BGRA8Unorm;
        /// Format of the passes' depth-stencil attachment, or Undefined when they have none. Carbon neither
        /// tests nor writes depth. Depth24Unorm and Depth24UnormStencil8 map to WebGPU's Depth24Plus formats.
        TextureFormat DepthStencilFormat = TextureFormat::Undefined;
        /// Sample count of the passes' attachments.
        uint32_t SampleCount = 1;
    };

    /// Installs the WebGPU backend into the current context. Returns false, and logs why, when the device is
    /// null, a format is not supported, or the context has a backend already.
    bool WebGPUInit(const WebGPUInitInfo& info);

    /// Removes the WebGPU backend from the current context and releases everything it created on the device.
    /// The context is headless afterwards. Does nothing when the context has no WebGPU backend.
    void WebGPUShutdown();

    /// Records the last finished frame into a render pass owned by the host. Call it after EndFrame, inside a
    /// pass whose attachments match the WebGPUInitInfo and whose size is the display size times the content
    /// scale. May be called more than once per frame, for several passes.
    ///
    /// Carbon uploads its buffers and glyph-atlas changes with the device's queue (Queue::WriteBuffer and
    /// WriteTexture), sets its own pipeline, bind groups, vertex and index buffers, viewport and scissor
    /// rectangle, and does not restore the previous ones. Draw the host's own content before, or set that state
    /// again afterwards.
    void WebGPURender(const wgpu::RenderPassEncoder& pass);

    /// Returns the TextureID of a host texture view, to draw it with Image or DrawList::AddImage. Carbon keeps a
    /// reference to the view while it is in use and releases it when a whole frame passes in which it was
    /// neither registered nor drawn. Textures are sampled with linear filtering and straight alpha.
    TextureID WebGPUGetTextureID(const wgpu::TextureView& view);

    /// Displays one of the host's texture views at `size` points: Image(WebGPUGetTextureID(view), ...).
    void WebGPUImage(const wgpu::TextureView& view, Vec2 size, const ImageOptions& options = {});
} // namespace Carbon
