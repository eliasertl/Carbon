#pragma once

#include <cstdint>
#include <unordered_map>

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <d3d9.h>
#include <wrl/client.h>

#include "Carbon/Backends/DX9/DX9Backend.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon::Internal
{
    /// The Direct3D 9 renderer backend.
    ///
    /// Shader model 3.0 cannot index a buffer from the pixel shader, so every vertex is written with its primitive
    /// copied into it (DX9Vertex). Vertices and indices go into dynamic buffers locked with D3DLOCK_DISCARD every
    /// frame. Those buffers, the glyph atlas and the state block live in D3DPOOL_DEFAULT: they are released by
    /// InvalidateDeviceObjects before the host resets the device, and created again when they are needed.
    class DX9Renderer : public RendererBackend
    {
    public:
        explicit DX9Renderer(const DX9InitInfo& info);

        /// False when the device lacks what Carbon needs or creating a shader failed; the reason was logged.
        bool IsValid() const { return m_IsValid; }

        std::string_view GetName() const override { return "Direct3D 9"; }
        RendererBackendCapabilities GetCapabilities() const override;
        void EndFrame() override { m_HasNewDrawData = true; }
        void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) override;
        void Render(const DrawData& drawData) override;
        void ReleaseTexture(TextureID texture) override;

        /// False while the device is lost; nothing may be drawn then.
        bool IsDeviceReady() const;

        /// Releases the D3DPOOL_DEFAULT objects and the host textures; see DX9InvalidateDeviceObjects.
        void InvalidateDeviceObjects();

        /// Registers a host texture for the current frame and returns its ID.
        TextureID RegisterTexture(IDirect3DTexture9* texture);

    private:
        template <typename T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;

        bool CreateShaders();
        bool EnsureVertexBuffer(UINT requiredSize);
        bool EnsureIndexBuffer(UINT requiredSize);

    private:
        ComPtr<IDirect3DDevice9> m_Device;
        D3DCAPS9 m_Caps = {};
        bool m_IsLinearOutput = false;
        bool m_IsValid = false;
        bool m_HasReportedIndexLimit = false;
        /// Set by EndFrame: the next Render has new draw data to upload. A frame that is rendered again (a window
        /// redrawn without a new frame) reuses what its first Render uploaded.
        bool m_HasNewDrawData = true;

        ComPtr<IDirect3DVertexShader9> m_VertexShader;
        ComPtr<IDirect3DPixelShader9> m_PixelShader;
        ComPtr<IDirect3DVertexDeclaration9> m_VertexDeclaration;

        // D3DPOOL_DEFAULT: released by InvalidateDeviceObjects.
        ComPtr<IDirect3DStateBlock9> m_StateBlock;
        ComPtr<IDirect3DVertexBuffer9> m_VertexBuffer;
        ComPtr<IDirect3DIndexBuffer9> m_IndexBuffer;
        UINT m_VertexCapacity = 0;
        UINT m_IndexCapacity = 0;
        /// One of the glyph atlases: L8 for coverage, A8R8G8B8 for color glyphs.
        struct AtlasTexture
        {
            ComPtr<IDirect3DTexture9> Texture;
            uint32_t Width = 0;
            uint32_t Height = 0;
        };
        AtlasTexture m_Atlas;
        AtlasTexture m_ColorAtlas;

        /// Host textures by key, each holding a reference while it is in use.
        std::unordered_map<uint64_t, ComPtr<IDirect3DTexture9>> m_HostTextures;
    };
} // namespace Carbon::Internal
