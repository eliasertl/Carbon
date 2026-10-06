#include "Carbon/Backends/DX11/DX11RendererInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"

namespace Carbon::Internal
{
    namespace
    {
        // Bytecode generated at build time by fxc from Shaders/Carbon.hlsl.
#include "Carbon/Backends/DX11/CarbonPixel.h"
#include "Carbon/Backends/DX11/CarbonVertex.h"

        // Mirrors the constant buffer `Frame` in the shader.
        struct FrameConstants
        {
            float DisplayWidth;
            float DisplayHeight;
            float ContentScale;
            float LinearOutput;
        };

        constexpr UINT MinimumBufferSize = 16 * 1024;

        bool Check(HRESULT result, const char* what)
        {
            if (SUCCEEDED(result))
                return true;
            CB_LOG_ERROR("DX11", "{} failed ({:#010x})", what, static_cast<uint32_t>(result));
            return false;
        }
    } // namespace

    DX11Renderer::DX11Renderer(const DX11InitInfo& info)
        : m_Device(info.Device), m_Context(info.Context), m_IsLinearOutput(IsSrgbFormat(info.ColorFormat))
    {
        m_CurrentContext = m_Context.Get();
        m_IsValid = CreateObjects();
    }

    bool DX11Renderer::CreateObjects()
    {
        ID3D11Device* device = m_Device.Get();
        if (!Check(device->CreateVertexShader(g_DX11VertexShader, sizeof(g_DX11VertexShader), nullptr, &m_VertexShader),
                   "CreateVertexShader") ||
            !Check(device->CreatePixelShader(g_DX11PixelShader, sizeof(g_DX11PixelShader), nullptr, &m_PixelShader),
                   "CreatePixelShader"))
            return false;

        // Vertex layout: mirrors DrawVertex.
        static_assert(sizeof(DrawVertex) == 32, "DrawVertex must match the input layout below");
        static_assert(sizeof(DrawPrimitive) == 32, "DrawPrimitive must be two R32G32B32A32_UINT texels");
        const std::array<D3D11_INPUT_ELEMENT_DESC, 5> elements = {
            D3D11_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(DrawVertex, Position),
                                     D3D11_INPUT_PER_VERTEX_DATA, 0},
            D3D11_INPUT_ELEMENT_DESC{"LOCAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(DrawVertex, Local),
                                     D3D11_INPUT_PER_VERTEX_DATA, 0},
            D3D11_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(DrawVertex, UV),
                                     D3D11_INPUT_PER_VERTEX_DATA, 0},
            D3D11_INPUT_ELEMENT_DESC{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(DrawVertex, Color),
                                     D3D11_INPUT_PER_VERTEX_DATA, 0},
            D3D11_INPUT_ELEMENT_DESC{"PRIMITIVE", 0, DXGI_FORMAT_R32_UINT, 0, offsetof(DrawVertex, Primitive),
                                     D3D11_INPUT_PER_VERTEX_DATA, 0}};
        if (!Check(device->CreateInputLayout(elements.data(), static_cast<UINT>(elements.size()), g_DX11VertexShader,
                                             sizeof(g_DX11VertexShader), &m_InputLayout),
                   "CreateInputLayout"))
            return false;

        D3D11_BUFFER_DESC frameDesc = {};
        frameDesc.ByteWidth = sizeof(FrameConstants);
        frameDesc.Usage = D3D11_USAGE_DYNAMIC;
        frameDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        frameDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (!Check(device->CreateBuffer(&frameDesc, nullptr, &m_FrameBuffer), "CreateBuffer (frame)"))
            return false;

        // Premultiplied alpha blending.
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (!Check(device->CreateBlendState(&blendDesc, &m_BlendState), "CreateBlendState"))
            return false;

        D3D11_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_NONE;
        rasterizerDesc.ScissorEnable = TRUE;
        rasterizerDesc.DepthClipEnable = TRUE;
        if (!Check(device->CreateRasterizerState(&rasterizerDesc, &m_RasterizerState), "CreateRasterizerState"))
            return false;

        // Carbon neither tests nor writes depth or stencil.
        D3D11_DEPTH_STENCIL_DESC depthDesc = {};
        depthDesc.DepthEnable = FALSE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        depthDesc.StencilEnable = FALSE;
        if (!Check(device->CreateDepthStencilState(&depthDesc, &m_DepthStencilState), "CreateDepthStencilState"))
            return false;

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerDesc.MaxLOD = 0.0f;
        return Check(device->CreateSamplerState(&samplerDesc, &m_Sampler), "CreateSamplerState");
    }

    bool DX11Renderer::EnsureBuffer(ComPtr<ID3D11Buffer>& buffer, UINT& capacity, UINT requiredSize, UINT bindFlags)
    {
        if (buffer != nullptr && capacity >= requiredSize)
            return true;

        // Grow geometrically so steady-state frames never reallocate.
        UINT newCapacity = std::max(capacity * 2, MinimumBufferSize);
        while (newCapacity < requiredSize)
            newCapacity *= 2;
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = newCapacity;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = bindFlags;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        buffer.Reset();
        if (!Check(m_Device->CreateBuffer(&desc, nullptr, &buffer), "CreateBuffer"))
        {
            capacity = 0;
            return false;
        }
        capacity = newCapacity;
        return true;
    }

    RendererBackendCapabilities DX11Renderer::GetCapabilities() const
    {
        // Feature level 10.0 guarantees 8192, 11.0 and later 16384; Carbon's own limit is lower than either.
        RendererBackendCapabilities capabilities;
        capabilities.MaxTextureSize = m_Device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ? 16384u : 8192u;
        return capabilities;
    }

    void DX11Renderer::UpdateGlyphAtlas(const GlyphAtlasUpdate& update)
    {
        // A full update may come with a new size; the changed rows of a partial one fit the texture there is.
        if (m_AtlasTexture == nullptr || m_AtlasWidth != update.Width || m_AtlasHeight != update.Height)
        {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = update.Width;
            desc.Height = update.Height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA data = {};
            data.pSysMem = update.Pixels.data();
            data.SysMemPitch = update.Width;
            m_AtlasView.Reset();
            m_AtlasTexture.Reset();
            if (!Check(m_Device->CreateTexture2D(&desc, &data, &m_AtlasTexture), "CreateTexture2D (glyph atlas)") ||
                !Check(m_Device->CreateShaderResourceView(m_AtlasTexture.Get(), nullptr, &m_AtlasView),
                       "CreateShaderResourceView (glyph atlas)"))
            {
                m_AtlasTexture.Reset();
                InvalidateGlyphAtlas();
                return;
            }
            m_AtlasWidth = update.Width;
            m_AtlasHeight = update.Height;
            return;
        }
        if (update.RowCount == 0)
            return;

        const D3D11_BOX box = {0, update.FirstRow, 0, update.Width, update.FirstRow + update.RowCount, 1};
        const uint8_t* rows = update.Pixels.data() + static_cast<size_t>(update.FirstRow) * update.Width;
        m_CurrentContext->UpdateSubresource(m_AtlasTexture.Get(), 0, &box, rows, update.Width, 0);
    }

    void DX11Renderer::SaveState(ID3D11DeviceContext* context, SavedState& state) const
    {
        state.ScissorCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        state.ViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        context->RSGetScissorRects(&state.ScissorCount, state.Scissors);
        context->RSGetViewports(&state.ViewportCount, state.Viewports);
        context->RSGetState(&state.Rasterizer);
        context->OMGetBlendState(&state.Blend, state.BlendFactor, &state.SampleMask);
        context->OMGetDepthStencilState(&state.DepthStencil, &state.StencilRef);
        ID3D11ShaderResourceView* resources[2] = {};
        context->PSGetShaderResources(0, 2, resources);
        state.PixelResources[0].Attach(resources[0]);
        state.PixelResources[1].Attach(resources[1]);
        context->PSGetSamplers(0, 1, &state.PixelSampler);
        // Class instances are not used by Carbon's shaders; the host's are restored with their shader.
        context->PSGetShader(&state.PixelShader, nullptr, nullptr);
        context->VSGetShader(&state.VertexShader, nullptr, nullptr);
        context->GSGetShader(&state.GeometryShader, nullptr, nullptr);
        context->VSGetConstantBuffers(0, 1, &state.VertexConstants);
        context->PSGetConstantBuffers(0, 1, &state.PixelConstants);
        context->IAGetPrimitiveTopology(&state.Topology);
        context->IAGetIndexBuffer(&state.IndexBuffer, &state.IndexFormat, &state.IndexOffset);
        context->IAGetVertexBuffers(0, 1, &state.VertexBuffer, &state.VertexStride, &state.VertexOffset);
        context->IAGetInputLayout(&state.InputLayout);
    }

    void DX11Renderer::RestoreState(ID3D11DeviceContext* context, SavedState& state)
    {
        context->RSSetScissorRects(state.ScissorCount, state.Scissors);
        context->RSSetViewports(state.ViewportCount, state.Viewports);
        context->RSSetState(state.Rasterizer.Get());
        context->OMSetBlendState(state.Blend.Get(), state.BlendFactor, state.SampleMask);
        context->OMSetDepthStencilState(state.DepthStencil.Get(), state.StencilRef);
        ID3D11ShaderResourceView* resources[2] = {state.PixelResources[0].Get(), state.PixelResources[1].Get()};
        context->PSSetShaderResources(0, 2, resources);
        ID3D11SamplerState* sampler = state.PixelSampler.Get();
        context->PSSetSamplers(0, 1, &sampler);
        context->PSSetShader(state.PixelShader.Get(), nullptr, 0);
        context->VSSetShader(state.VertexShader.Get(), nullptr, 0);
        context->GSSetShader(state.GeometryShader.Get(), nullptr, 0);
        ID3D11Buffer* vertexConstants = state.VertexConstants.Get();
        context->VSSetConstantBuffers(0, 1, &vertexConstants);
        ID3D11Buffer* pixelConstants = state.PixelConstants.Get();
        context->PSSetConstantBuffers(0, 1, &pixelConstants);
        context->IASetPrimitiveTopology(state.Topology);
        context->IASetIndexBuffer(state.IndexBuffer.Get(), state.IndexFormat, state.IndexOffset);
        ID3D11Buffer* vertexBuffer = state.VertexBuffer.Get();
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &state.VertexStride, &state.VertexOffset);
        context->IASetInputLayout(state.InputLayout.Get());
    }

    void DX11Renderer::Render(const DrawData& drawData)
    {
        ID3D11DeviceContext* context = m_CurrentContext;
        if (m_AtlasView == nullptr)
            return;

        // Buffers: written whole once per frame, discarding what the GPU may still read.
        const UINT vertexBytes = static_cast<UINT>(drawData.Vertices.size_bytes());
        const UINT indexBytes = static_cast<UINT>(drawData.Indices.size_bytes());
        const UINT primitiveBytes =
            static_cast<UINT>(std::max(drawData.Primitives.size_bytes(), sizeof(DrawPrimitive)));
        const ID3D11Buffer* oldVertices = m_VertexBuffer.Get();
        const ID3D11Buffer* oldIndices = m_IndexBuffer.Get();
        const ID3D11Buffer* oldPrimitives = m_PrimitiveBuffer.Get();
        if (!EnsureBuffer(m_VertexBuffer, m_VertexCapacity, vertexBytes, D3D11_BIND_VERTEX_BUFFER) ||
            !EnsureBuffer(m_IndexBuffer, m_IndexCapacity, indexBytes, D3D11_BIND_INDEX_BUFFER) ||
            !EnsureBuffer(m_PrimitiveBuffer, m_PrimitiveCapacity, primitiveBytes, D3D11_BIND_SHADER_RESOURCE))
            return;
        if (m_PrimitiveBuffer.Get() != oldPrimitives || m_PrimitiveView == nullptr)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
            viewDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
            viewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            viewDesc.Buffer.FirstElement = 0;
            viewDesc.Buffer.NumElements = m_PrimitiveCapacity / 16;
            m_PrimitiveView.Reset();
            if (!Check(m_Device->CreateShaderResourceView(m_PrimitiveBuffer.Get(), &viewDesc, &m_PrimitiveView),
                       "CreateShaderResourceView (primitives)"))
                return;
        }

        const float scale = drawData.ContentScale;
        const bool hasNewBuffers = m_VertexBuffer.Get() != oldVertices || m_IndexBuffer.Get() != oldIndices ||
                                   m_PrimitiveBuffer.Get() != oldPrimitives;
        if (m_HasNewDrawData || hasNewBuffers || m_UploadContext != context)
        {
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (!Check(context->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Map (vertices)"))
                return;
            std::memcpy(mapped.pData, drawData.Vertices.data(), vertexBytes);
            context->Unmap(m_VertexBuffer.Get(), 0);
            if (!Check(context->Map(m_IndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Map (indices)"))
                return;
            std::memcpy(mapped.pData, drawData.Indices.data(), indexBytes);
            context->Unmap(m_IndexBuffer.Get(), 0);
            if (!Check(context->Map(m_PrimitiveBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
                       "Map (primitives)"))
                return;
            if (!drawData.Primitives.empty())
                std::memcpy(mapped.pData, drawData.Primitives.data(), drawData.Primitives.size_bytes());
            context->Unmap(m_PrimitiveBuffer.Get(), 0);

            const FrameConstants constants = {drawData.DisplaySize.X, drawData.DisplaySize.Y, scale,
                                              m_IsLinearOutput ? 1.0f : 0.0f};
            if (!Check(context->Map(m_FrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Map (frame)"))
                return;
            std::memcpy(mapped.pData, &constants, sizeof(constants));
            context->Unmap(m_FrameBuffer.Get(), 0);
            m_HasNewDrawData = false;
            m_UploadContext = context;
        }

        SavedState state;
        SaveState(context, state);

        // Carbon's pipeline.
        const float targetWidth = std::round(drawData.DisplaySize.X * scale);
        const float targetHeight = std::round(drawData.DisplaySize.Y * scale);
        const D3D11_VIEWPORT viewport = {0.0f, 0.0f, targetWidth, targetHeight, 0.0f, 1.0f};
        context->RSSetViewports(1, &viewport);
        context->RSSetState(m_RasterizerState.Get());
        const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context->OMSetBlendState(m_BlendState.Get(), blendFactor, 0xFFFFFFFF);
        context->OMSetDepthStencilState(m_DepthStencilState.Get(), 0);
        context->IASetInputLayout(m_InputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const UINT stride = sizeof(DrawVertex);
        const UINT offset = 0;
        ID3D11Buffer* vertexBuffer = m_VertexBuffer.Get();
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
        ID3D11Buffer* frameBuffer = m_FrameBuffer.Get();
        context->VSSetConstantBuffers(0, 1, &frameBuffer);
        context->PSSetConstantBuffers(0, 1, &frameBuffer);
        ID3D11SamplerState* sampler = m_Sampler.Get();
        context->PSSetSamplers(0, 1, &sampler);
        ID3D11ShaderResourceView* primitiveView = m_PrimitiveView.Get();
        context->PSSetShaderResources(1, 1, &primitiveView);

        ID3D11ShaderResourceView* boundView = nullptr;
        for (const DrawCommand& command : drawData.Commands)
        {
            // Clip rectangles are in points; the scissor rectangle is in pixels and must stay inside the target.
            const float left = std::clamp(std::floor(command.ClipRect.X * scale + 0.5f), 0.0f, targetWidth);
            const float top = std::clamp(std::floor(command.ClipRect.Y * scale + 0.5f), 0.0f, targetHeight);
            const float right = std::clamp(std::floor(command.ClipRect.GetRight() * scale + 0.5f), 0.0f, targetWidth);
            const float bottom =
                std::clamp(std::floor(command.ClipRect.GetBottom() * scale + 0.5f), 0.0f, targetHeight);
            if (right <= left || bottom <= top)
                continue;

            ID3D11ShaderResourceView* view = m_AtlasView.Get();
            if (command.Texture != TextureID())
            {
                // A view not registered with DX11GetTextureID is a raw handle (MakeTextureID): Carbon takes its
                // reference now.
                ComPtr<ID3D11ShaderResourceView>& texture = m_HostTextures[command.Texture.Value];
                if (texture == nullptr)
                    texture =
                        reinterpret_cast<ID3D11ShaderResourceView*>(static_cast<uintptr_t>(command.Texture.Value));
                view = texture.Get();
            }
            if (view != boundView)
            {
                context->PSSetShaderResources(0, 1, &view);
                boundView = view;
            }

            const D3D11_RECT scissor = {static_cast<LONG>(left), static_cast<LONG>(top), static_cast<LONG>(right),
                                        static_cast<LONG>(bottom)};
            context->RSSetScissorRects(1, &scissor);
            context->DrawIndexed(command.IndexCount, command.IndexOffset, 0);
        }

        RestoreState(context, state);
    }

    TextureID DX11Renderer::RegisterTexture(ID3D11ShaderResourceView* view)
    {
        if (view == nullptr)
            return TextureID();
        const uint64_t key = MakeTextureID(view).Value;
        ComPtr<ID3D11ShaderResourceView>& texture = m_HostTextures[key];
        if (texture == nullptr)
            texture = view; // adds a reference
        return RegisterHostTexture(key);
    }

    void DX11Renderer::ReleaseTexture(TextureID texture)
    {
        m_HostTextures.erase(texture.Value);
    }
} // namespace Carbon::Internal
