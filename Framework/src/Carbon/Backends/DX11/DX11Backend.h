#pragma once

#include <cstdint>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Renderer/TextureFormat.h"
#include "Carbon/Widgets/Image.h"

/// The Direct3D 11 renderer backend, for feature level 10.0 and later. Available on Windows when Carbon was built with
/// CARBON_BACKEND_DX11 (then CARBON_HAS_BACKEND_DX11 is defined). See Docs/Backends.md.
///
/// The interfaces are only declared here, so this header pulls in no Windows header; include d3d11.h yourself.

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace Carbon
{
    /// What the Direct3D 11 backend needs from the host.
    struct DX11InitInfo
    {
        /// The device Carbon creates its shaders, states, buffers and glyph atlas on. Carbon holds a reference.
        ID3D11Device* Device = nullptr;
        /// The context DX11Render records into unless it is given another one (usually the immediate context).
        /// Carbon holds a reference.
        ID3D11DeviceContext* Context = nullptr;
        /// Format of the render targets Carbon draws into. Only whether it is sRGB matters: with an sRGB render
        /// target view Carbon writes linear values, which Direct3D encodes.
        TextureFormat ColorFormat = TextureFormat::BGRA8Unorm;
    };

    /// Installs the Direct3D 11 backend into the current context. Returns false, and logs why, when the device or
    /// context is missing, an object could not be created, or the context has a backend already.
    bool DX11Init(const DX11InitInfo& info);

    /// Removes the Direct3D 11 backend from the current context and releases everything it created and every
    /// reference it holds.
    void DX11Shutdown();

    /// Draws the last finished frame into the render target bound to `context` (OMSetRenderTargets), whose size is
    /// the display size times the content scale. Call it after EndFrame. `context` may be a deferred context; null
    /// uses the context from DX11InitInfo. Glyph-atlas changes are uploaded with the same context.
    ///
    /// Carbon saves every piece of pipeline state it changes and restores it before returning: input layout,
    /// topology, vertex and index buffers, vertex, geometry and pixel shaders, their constant buffer, shader
    /// resources (slots 0 and 1) and sampler (slot 0), rasterizer, blend and depth-stencil states, viewports and
    /// scissor rectangles. The render target stays as the host bound it.
    void DX11Render(ID3D11DeviceContext* context = nullptr);

    /// Returns the TextureID of a host shader resource view, to draw it with Image or DrawList::AddImage.
    /// MakeTextureID(view) gives the same ID without registering it. Carbon holds a reference to the view while it
    /// is in use and releases it when a whole frame passes in which it was neither registered nor drawn. Views are
    /// sampled with linear filtering and straight alpha.
    TextureID DX11GetTextureID(ID3D11ShaderResourceView* view);

    /// Displays one of the host's shader resource views at `size` points: Image(DX11GetTextureID(view), ...).
    void DX11Image(ID3D11ShaderResourceView* view, Vec2 size, const ImageOptions& options = {});
} // namespace Carbon
