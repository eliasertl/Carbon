#pragma once

#include <webgpu/webgpu_cpp.h>

#include "Carbon/Draw/DrawTypes.h"

namespace Carbon
{
    /// Records the draw data of the last finished frame into a render pass owned by the host. Call it after
    /// EndFrame, inside a pass whose color attachment has the format given in the ContextDescription and whose
    /// size is the display size times the content scale.
    ///
    /// Carbon sets its own pipeline, bind groups, vertex and index buffers, viewport and scissor rectangle and
    /// does not restore the previous ones. Draw the host's own content before calling Render, or set that state
    /// again afterwards.
    void Render(const wgpu::RenderPassEncoder& pass);

    /// Returns the TextureID for a host texture view so it can be drawn with DrawList::AddImage. Carbon keeps a
    /// reference to the view while it is in use and releases it when a whole frame passes without it being drawn,
    /// so either call this every frame or keep drawing the ID you got.
    TextureID GetTextureID(const wgpu::TextureView& view);
} // namespace Carbon
