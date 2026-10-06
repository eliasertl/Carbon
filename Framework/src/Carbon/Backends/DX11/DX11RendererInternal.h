#pragma once

#include <cstdint>
#include <unordered_map>

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl/client.h>

#include "Carbon/Backends/DX11/DX11Backend.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon::Internal
{
    /// The Direct3D 11 renderer backend.
    ///
    /// Vertices, indices and primitives go into dynamic buffers mapped with WRITE_DISCARD every frame, so the driver
    /// hands out fresh memory while the GPU still reads the previous frame's. Primitives are a typed buffer of
    /// R32G32B32A32_UINT texels, two per primitive. Every Render saves the pipeline state it changes and restores it.
    class DX11Renderer : public RendererBackend
    {
    public:
        explicit DX11Renderer(const DX11InitInfo& info);

        /// False when creating a shader, state or buffer failed; the reason was logged.
        bool IsValid() const { return m_IsValid; }

        std::string_view GetName() const override { return "Direct3D 11"; }
        RendererBackendCapabilities GetCapabilities() const override;
        void EndFrame() override { m_HasNewDrawData = true; }
        void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) override;
        void Render(const DrawData& drawData) override;
        void ReleaseTexture(TextureID texture) override;

        /// The context the next UpdateGlyphAtlas and Render calls use; null returns to the init context.
        void SetContext(ID3D11DeviceContext* context)
        {
            m_CurrentContext = context != nullptr ? context : m_Context.Get();
        }

        /// Registers a host shader resource view for the current frame and returns its ID.
        TextureID RegisterTexture(ID3D11ShaderResourceView* view);

    private:
        template <typename T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;

        /// The pipeline state Carbon changes, as found before it does. Getters add references, which ComPtr releases.
        struct SavedState
        {
            UINT ScissorCount = 0;
            UINT ViewportCount = 0;
            D3D11_RECT Scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            ComPtr<ID3D11RasterizerState> Rasterizer;
            ComPtr<ID3D11BlendState> Blend;
            FLOAT BlendFactor[4] = {};
            UINT SampleMask = 0;
            ComPtr<ID3D11DepthStencilState> DepthStencil;
            UINT StencilRef = 0;
            ComPtr<ID3D11ShaderResourceView> PixelResources[2];
            ComPtr<ID3D11SamplerState> PixelSampler;
            ComPtr<ID3D11PixelShader> PixelShader;
            ComPtr<ID3D11VertexShader> VertexShader;
            ComPtr<ID3D11GeometryShader> GeometryShader;
            ComPtr<ID3D11Buffer> VertexConstants;
            ComPtr<ID3D11Buffer> PixelConstants;
            D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
            ComPtr<ID3D11Buffer> IndexBuffer;
            DXGI_FORMAT IndexFormat = DXGI_FORMAT_UNKNOWN;
            UINT IndexOffset = 0;
            ComPtr<ID3D11Buffer> VertexBuffer;
            UINT VertexStride = 0;
            UINT VertexOffset = 0;
            ComPtr<ID3D11InputLayout> InputLayout;
        };

        bool CreateObjects();
        bool EnsureBuffer(ComPtr<ID3D11Buffer>& buffer, UINT& capacity, UINT requiredSize, UINT bindFlags);
        void SaveState(ID3D11DeviceContext* context, SavedState& state) const;
        static void RestoreState(ID3D11DeviceContext* context, SavedState& state);

    private:
        ComPtr<ID3D11Device> m_Device;
        ComPtr<ID3D11DeviceContext> m_Context;
        ID3D11DeviceContext* m_CurrentContext = nullptr;
        bool m_IsLinearOutput = false;
        bool m_IsValid = false;
        /// Set by EndFrame: the next Render has new draw data to upload. A frame that is rendered again (a window
        /// redrawn without a new frame) reuses what its first Render uploaded.
        bool m_HasNewDrawData = true;
        /// The context the frame's data was uploaded through. Another one (a deferred context) uploads again:
        /// what it records may run before the first one's commands.
        ID3D11DeviceContext* m_UploadContext = nullptr;

        ComPtr<ID3D11VertexShader> m_VertexShader;
        ComPtr<ID3D11PixelShader> m_PixelShader;
        ComPtr<ID3D11InputLayout> m_InputLayout;
        ComPtr<ID3D11Buffer> m_FrameBuffer;
        ComPtr<ID3D11BlendState> m_BlendState;
        ComPtr<ID3D11RasterizerState> m_RasterizerState;
        ComPtr<ID3D11DepthStencilState> m_DepthStencilState;
        ComPtr<ID3D11SamplerState> m_Sampler;

        ComPtr<ID3D11Buffer> m_VertexBuffer;
        ComPtr<ID3D11Buffer> m_IndexBuffer;
        ComPtr<ID3D11Buffer> m_PrimitiveBuffer;
        ComPtr<ID3D11ShaderResourceView> m_PrimitiveView;
        UINT m_VertexCapacity = 0;
        UINT m_IndexCapacity = 0;
        UINT m_PrimitiveCapacity = 0;

        ComPtr<ID3D11Texture2D> m_AtlasTexture;
        ComPtr<ID3D11ShaderResourceView> m_AtlasView;
        uint32_t m_AtlasWidth = 0;
        uint32_t m_AtlasHeight = 0;

        /// Host views by key, each holding a reference while it is in use.
        std::unordered_map<uint64_t, ComPtr<ID3D11ShaderResourceView>> m_HostTextures;
    };
} // namespace Carbon::Internal
