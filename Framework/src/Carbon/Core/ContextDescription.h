#pragma once

#include <cstdint>

#include <webgpu/webgpu_cpp.h>

#include "Carbon/Core/Callbacks.h"

namespace Carbon
{
    /// Everything needed to create a context. The host owns the device and every surface or texture Carbon
    /// renders into.
    struct ContextDescription
    {
        /// The device Carbon creates its pipeline, buffers and glyph atlas on. May be null for a headless context
        /// that builds draw data but cannot render (unit tests, tools).
        wgpu::Device Device;
        /// Format of the color attachment of the pass given to Render.
        wgpu::TextureFormat ColorFormat = wgpu::TextureFormat::BGRA8Unorm;
        /// Format of the pass's depth-stencil attachment, or Undefined when it has none.
        wgpu::TextureFormat DepthStencilFormat = wgpu::TextureFormat::Undefined;
        /// Sample count of the pass's attachments.
        uint32_t SampleCount = 1;
        /// Host hooks: logging, clipboard, cursor, asserts.
        Carbon::Callbacks Callbacks;
    };
} // namespace Carbon
