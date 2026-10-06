#pragma once

#include <cstdint>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Renderer/TextureFormat.h"
#include "Carbon/Widgets/Image.h"

/// The Direct3D 9 renderer backend, for devices with shader model 3.0. Available on Windows when Carbon was built with
/// CARBON_BACKEND_DX9 (then CARBON_HAS_BACKEND_DX9 is defined). See Docs/Backends.md.
///
/// The interfaces are only declared here, so this header pulls in no Windows header; include d3d9.h yourself.

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace Carbon
{
    /// What the Direct3D 9 backend needs from the host.
    struct DX9InitInfo
    {
        /// The device Carbon draws with. It needs vertex and pixel shader 3.0 and 32-bit indices. Carbon holds a
        /// reference. A Direct3D 9Ex device works as well.
        IDirect3DDevice9* Device = nullptr;
        /// Format of the render targets Carbon draws into. Only whether it is sRGB matters: for an sRGB format Carbon
        /// writes linear values with D3DRS_SRGBWRITEENABLE, so that Direct3D encodes them.
        TextureFormat ColorFormat = TextureFormat::BGRA8Unorm;
    };

    /// Installs the Direct3D 9 backend into the current context. Returns false, and logs why, when the device is
    /// missing or lacks shader model 3.0, a shader could not be created, or the context has a backend already.
    bool DX9Init(const DX9InitInfo& info);

    /// Removes the Direct3D 9 backend from the current context and releases everything it created and every
    /// reference it holds.
    void DX9Shutdown();

    /// Draws the last finished frame into render target 0, whose size is the display size times the content scale.
    /// Call it after EndFrame, between the host's BeginScene and EndScene. Draws nothing while the device is lost.
    ///
    /// Carbon captures the device's state in a state block before it draws and applies it again afterwards, and
    /// restores the viewport and scissor rectangle, so the host's state is unchanged.
    void DX9Render();

    /// Releases what Carbon keeps in D3DPOOL_DEFAULT (its buffers, the glyph atlas, its state block) and its
    /// references to host textures. Call it before IDirect3DDevice9::Reset; Carbon creates the objects again in the
    /// next DX9Render.
    void DX9InvalidateDeviceObjects();

    /// Returns the TextureID of a host texture, to draw it with Image or DrawList::AddImage. MakeTextureID(texture)
    /// gives the same ID without registering it. Carbon holds a reference to the texture while it is in use and
    /// releases it when a whole frame passes in which it was neither registered nor drawn, or when
    /// DX9InvalidateDeviceObjects is called. Level 0 is sampled with linear filtering and straight alpha.
    TextureID DX9GetTextureID(IDirect3DTexture9* texture);

    /// Displays one of the host's textures at `size` points: Image(DX9GetTextureID(texture), ...).
    void DX9Image(IDirect3DTexture9* texture, Vec2 size, const ImageOptions& options = {});
} // namespace Carbon
