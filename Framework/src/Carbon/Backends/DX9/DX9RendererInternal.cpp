#include "Carbon/Backends/DX9/DX9RendererInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"

namespace Carbon::Internal
{
    namespace
    {
        // Bytecode generated at build time by fxc from Shaders/Carbon.hlsl.
#include "Carbon/Backends/DX9/CarbonPixel.h"
#include "Carbon/Backends/DX9/CarbonVertex.h"

        /// A DrawVertex with its primitive copied in, as the vertex shader reads it.
        struct DX9Vertex
        {
            float Position[2];
            float Local[2];
            float UV[2];
            /// D3DCOLOR: 0xAARRGGBB.
            uint32_t Color;
            /// Half size, radius, smoothing.
            float Shape[4];
            /// Stroke width, softness, kind.
            float Stroke[3];
        };
        static_assert(sizeof(DX9Vertex) == 56, "DX9Vertex must match the vertex declaration below");

        constexpr UINT MinimumBufferSize = 16 * 1024;

        bool Check(HRESULT result, const char* what)
        {
            if (SUCCEEDED(result))
                return true;
            CB_LOG_ERROR("DX9", "{} failed ({:#010x})", what, static_cast<uint32_t>(result));
            return false;
        }

        /// The capacity for `requiredSize` bytes: doubled from `capacity` so that steady-state frames never
        /// reallocate.
        UINT GrowCapacity(UINT capacity, UINT requiredSize)
        {
            UINT newCapacity = std::max(capacity * 2, MinimumBufferSize);
            while (newCapacity < requiredSize)
                newCapacity *= 2;
            return newCapacity;
        }

        /// Carbon's colors are R, G, B, A in memory; D3DCOLOR is B, G, R, A.
        uint32_t ToD3DColor(uint32_t rgba)
        {
            return (rgba & 0xFF00FF00u) | ((rgba & 0xFFu) << 16) | ((rgba >> 16) & 0xFFu);
        }
    } // namespace

    DX9Renderer::DX9Renderer(const DX9InitInfo& info)
        : m_Device(info.Device), m_IsLinearOutput(IsSrgbFormat(info.ColorFormat))
    {
        if (!Check(m_Device->GetDeviceCaps(&m_Caps), "GetDeviceCaps"))
            return;
        if (m_Caps.VertexShaderVersion < D3DVS_VERSION(3, 0) || m_Caps.PixelShaderVersion < D3DPS_VERSION(3, 0))
        {
            CB_LOG_ERROR("DX9", "The device lacks shader model 3.0, which Carbon needs");
            return;
        }
        if (m_Caps.MaxVertexIndex <= 0xFFFF)
        {
            CB_LOG_ERROR("DX9", "The device lacks 32-bit indices, which Carbon needs");
            return;
        }
        m_IsValid = CreateShaders();
    }

    bool DX9Renderer::CreateShaders()
    {
        IDirect3DDevice9* device = m_Device.Get();
        if (!Check(device->CreateVertexShader(reinterpret_cast<const DWORD*>(g_DX9VertexShader), &m_VertexShader),
                   "CreateVertexShader") ||
            !Check(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_DX9PixelShader), &m_PixelShader),
                   "CreatePixelShader"))
            return false;

        const std::array<D3DVERTEXELEMENT9, 7> elements = {
            D3DVERTEXELEMENT9{0, offsetof(DX9Vertex, Position), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
                              D3DDECLUSAGE_POSITION, 0},
            D3DVERTEXELEMENT9{0, offsetof(DX9Vertex, Local), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
                              D3DDECLUSAGE_TEXCOORD, 0},
            D3DVERTEXELEMENT9{0, offsetof(DX9Vertex, UV), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
                              D3DDECLUSAGE_TEXCOORD, 1},
            D3DVERTEXELEMENT9{0, offsetof(DX9Vertex, Color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
                              D3DDECLUSAGE_COLOR, 0},
            D3DVERTEXELEMENT9{0, offsetof(DX9Vertex, Shape), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,
                              D3DDECLUSAGE_TEXCOORD, 2},
            D3DVERTEXELEMENT9{0, offsetof(DX9Vertex, Stroke), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
                              D3DDECLUSAGE_TEXCOORD, 3},
            D3DVERTEXELEMENT9{0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0}}; // D3DDECL_END
        return Check(device->CreateVertexDeclaration(elements.data(), &m_VertexDeclaration), "CreateVertexDeclaration");
    }

    bool DX9Renderer::EnsureVertexBuffer(UINT requiredSize)
    {
        if (m_VertexBuffer != nullptr && m_VertexCapacity >= requiredSize)
            return true;
        const UINT capacity = GrowCapacity(m_VertexCapacity, requiredSize);
        m_VertexBuffer.Reset();
        m_VertexCapacity = 0;
        if (!Check(m_Device->CreateVertexBuffer(capacity, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
                                                &m_VertexBuffer, nullptr),
                   "CreateVertexBuffer"))
            return false;
        m_VertexCapacity = capacity;
        return true;
    }

    bool DX9Renderer::EnsureIndexBuffer(UINT requiredSize)
    {
        if (m_IndexBuffer != nullptr && m_IndexCapacity >= requiredSize)
            return true;
        const UINT capacity = GrowCapacity(m_IndexCapacity, requiredSize);
        m_IndexBuffer.Reset();
        m_IndexCapacity = 0;
        if (!Check(m_Device->CreateIndexBuffer(capacity, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32,
                                               D3DPOOL_DEFAULT, &m_IndexBuffer, nullptr),
                   "CreateIndexBuffer"))
            return false;
        m_IndexCapacity = capacity;
        return true;
    }

    RendererBackendCapabilities DX9Renderer::GetCapabilities() const
    {
        RendererBackendCapabilities capabilities;
        capabilities.MaxTextureSize = static_cast<uint32_t>(std::min(m_Caps.MaxTextureWidth, m_Caps.MaxTextureHeight));
        return capabilities;
    }

    bool DX9Renderer::IsDeviceReady() const
    {
        return m_Device->TestCooperativeLevel() == D3D_OK;
    }

    void DX9Renderer::UpdateGlyphAtlas(const GlyphAtlasUpdate& update)
    {
        const bool isColor = update.Format == GlyphAtlasFormat::Color;
        AtlasTexture& atlas = isColor ? m_ColorAtlas : m_Atlas;

        // A new size gets a new texture; a lost one (InvalidateDeviceObjects) is created again with a full update.
        bool isNew = false;
        if (atlas.Texture == nullptr || atlas.Width != update.Width || atlas.Height != update.Height)
        {
            atlas = AtlasTexture();
            if (!update.IsFull)
            {
                InvalidateGlyphAtlas();
                return;
            }
            const D3DFORMAT format = isColor ? D3DFMT_A8R8G8B8 : D3DFMT_L8;
            if (!Check(m_Device->CreateTexture(update.Width, update.Height, 1, D3DUSAGE_DYNAMIC, format,
                                               D3DPOOL_DEFAULT, &atlas.Texture, nullptr),
                       "CreateTexture (glyph atlas)"))
            {
                InvalidateGlyphAtlas();
                return;
            }
            atlas.Width = update.Width;
            atlas.Height = update.Height;
            isNew = true;
        }

        const bool isWhole = isNew || update.IsFull;
        const uint32_t firstRow = isWhole ? 0 : update.FirstRow;
        const uint32_t rowCount = isWhole ? update.Height : update.RowCount;
        if (rowCount == 0)
            return;
        const RECT rows = {0, static_cast<LONG>(firstRow), static_cast<LONG>(update.Width),
                           static_cast<LONG>(firstRow + rowCount)};
        D3DLOCKED_RECT locked = {};
        if (!Check(atlas.Texture->LockRect(0, &locked, isWhole ? nullptr : &rows, isWhole ? D3DLOCK_DISCARD : 0),
                   "LockRect (glyph atlas)"))
        {
            InvalidateGlyphAtlas();
            return;
        }
        const size_t rowBytes = static_cast<size_t>(update.Width) * (isColor ? 4 : 1);
        for (uint32_t row = 0; row < rowCount; row++)
        {
            uint8_t* target = static_cast<uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch;
            const uint8_t* source = update.Pixels.data() + static_cast<size_t>(firstRow + row) * rowBytes;
            if (!isColor)
            {
                std::memcpy(target, source, rowBytes);
                continue;
            }
            // A8R8G8B8 is stored as B, G, R, A; the update is R, G, B, A.
            for (size_t texel = 0; texel < rowBytes; texel += 4)
            {
                target[texel + 0] = source[texel + 2];
                target[texel + 1] = source[texel + 1];
                target[texel + 2] = source[texel + 0];
                target[texel + 3] = source[texel + 3];
            }
        }
        atlas.Texture->UnlockRect(0);
    }

    void DX9Renderer::Render(const DrawData& drawData)
    {
        IDirect3DDevice9* device = m_Device.Get();
        if (m_Atlas.Texture == nullptr)
            return;
        if (drawData.Vertices.size() - 1 > m_Caps.MaxVertexIndex)
        {
            if (!m_HasReportedIndexLimit)
                CB_LOG_ERROR("DX9", "A frame of {} vertices exceeds the device's limit of {}; it is not drawn",
                             drawData.Vertices.size(), m_Caps.MaxVertexIndex + 1);
            m_HasReportedIndexLimit = true;
            return;
        }

        // Buffers: written whole once per frame, discarding what the GPU may still read. A frame that is rendered
        // again finds its vertices, with the primitives copied into them, where its first Render put them.
        const UINT vertexBytes = static_cast<UINT>(drawData.Vertices.size() * sizeof(DX9Vertex));
        const UINT indexBytes = static_cast<UINT>(drawData.Indices.size_bytes());
        if (!EnsureVertexBuffer(vertexBytes) || !EnsureIndexBuffer(indexBytes))
            return;
        if (m_HasNewDrawData)
        {
            void* mapped = nullptr;
            if (!Check(m_VertexBuffer->Lock(0, vertexBytes, &mapped, D3DLOCK_DISCARD), "Lock (vertices)"))
                return;
            DX9Vertex* vertices = static_cast<DX9Vertex*>(mapped);
            const DrawPrimitive empty;
            for (size_t i = 0; i < drawData.Vertices.size(); i++)
            {
                const DrawVertex& source = drawData.Vertices[i];
                const DrawPrimitive& primitive =
                    source.Primitive < drawData.Primitives.size() ? drawData.Primitives[source.Primitive] : empty;
                vertices[i] =
                    DX9Vertex{{source.Position.X, source.Position.Y},
                              {source.Local.X, source.Local.Y},
                              {source.UV.X, source.UV.Y},
                              ToD3DColor(source.Color),
                              {primitive.HalfSize.X, primitive.HalfSize.Y, primitive.Radius, primitive.Smoothing},
                              {primitive.StrokeWidth, primitive.Softness, static_cast<float>(primitive.Kind)}};
            }
            m_VertexBuffer->Unlock();
            if (!Check(m_IndexBuffer->Lock(0, indexBytes, &mapped, D3DLOCK_DISCARD), "Lock (indices)"))
                return;
            std::memcpy(mapped, drawData.Indices.data(), indexBytes);
            m_IndexBuffer->Unlock();
            m_HasNewDrawData = false;
        }

        // Everything the host set: a state block captures it, and the viewport and scissor rectangle are kept
        // separately, as not every driver restores them with the block.
        if (m_StateBlock == nullptr)
        {
            if (!Check(device->CreateStateBlock(D3DSBT_ALL, &m_StateBlock), "CreateStateBlock"))
                return;
        }
        else if (!Check(m_StateBlock->Capture(), "IDirect3DStateBlock9::Capture"))
        {
            return;
        }
        D3DVIEWPORT9 savedViewport = {};
        RECT savedScissor = {};
        device->GetViewport(&savedViewport);
        device->GetScissorRect(&savedScissor);

        // Carbon's pipeline.
        const float scale = drawData.ContentScale;
        const float targetWidth = std::round(drawData.DisplaySize.X * scale);
        const float targetHeight = std::round(drawData.DisplaySize.Y * scale);
        const D3DVIEWPORT9 viewport = {0,    0,   static_cast<DWORD>(targetWidth), static_cast<DWORD>(targetHeight),
                                       0.0f, 1.0f};
        device->SetViewport(&viewport);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, m_IsLinearOutput ? TRUE : FALSE);
        // Premultiplied alpha blending.
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
        device->SetVertexDeclaration(m_VertexDeclaration.Get());
        device->SetStreamSource(0, m_VertexBuffer.Get(), 0, sizeof(DX9Vertex));
        device->SetStreamSourceFreq(0, 1);
        device->SetIndices(m_IndexBuffer.Get());
        device->SetVertexShader(m_VertexShader.Get());
        device->SetPixelShader(m_PixelShader.Get());
        // Direct3D 9 puts pixel centers on integer coordinates; moving everything up and left by half a pixel
        // matches the other APIs, whose centers are at half-integers.
        const float frame[4] = {drawData.DisplaySize.X, drawData.DisplaySize.Y, -1.0f / targetWidth,
                                1.0f / targetHeight};
        // The last two values are one over the size of the glyph atlas bound, set with the texture below.
        float output[4] = {scale, m_IsLinearOutput ? 1.0f : 0.0f, 1.0f / static_cast<float>(m_Atlas.Width),
                           1.0f / static_cast<float>(m_Atlas.Height)};
        device->SetVertexShaderConstantF(0, frame, 1);
        device->SetVertexShaderConstantF(1, output, 1);
        device->SetPixelShaderConstantF(0, frame, 1);
        device->SetPixelShaderConstantF(1, output, 1);
        const AtlasTexture* boundAtlas = &m_Atlas;

        const UINT vertexCount = static_cast<UINT>(drawData.Vertices.size());
        IDirect3DTexture9* boundTexture = nullptr;
        for (const DrawCommand& command : drawData.Commands)
        {
            // Clip rectangles are in points; the scissor rectangle is in pixels and must stay inside the target.
            const float left = std::clamp(std::floor(command.ClipRect.X * scale + 0.5f), 0.0f, targetWidth);
            const float top = std::clamp(std::floor(command.ClipRect.Y * scale + 0.5f), 0.0f, targetHeight);
            const float right = std::clamp(std::floor(command.ClipRect.GetRight() * scale + 0.5f), 0.0f, targetWidth);
            const float bottom =
                std::clamp(std::floor(command.ClipRect.GetBottom() * scale + 0.5f), 0.0f, targetHeight);
            if (right <= left || bottom <= top || command.IndexCount < 3)
                continue;

            IDirect3DTexture9* texture = m_Atlas.Texture.Get();
            const AtlasTexture* atlas = &m_Atlas;
            if (command.Texture == ColorGlyphAtlasTextureID)
            {
                // Sent before any command samples it; without it there is nothing to draw.
                if (m_ColorAtlas.Texture == nullptr)
                    continue;
                texture = m_ColorAtlas.Texture.Get();
                atlas = &m_ColorAtlas;
            }
            else if (command.Texture != TextureID())
            {
                // A texture not registered with DX9GetTextureID is a raw handle (MakeTextureID): Carbon takes its
                // reference now.
                ComPtr<IDirect3DTexture9>& hostTexture = m_HostTextures[command.Texture.Value];
                if (hostTexture == nullptr)
                    hostTexture = reinterpret_cast<IDirect3DTexture9*>(static_cast<uintptr_t>(command.Texture.Value));
                texture = hostTexture.Get();
            }
            if (texture != boundTexture)
            {
                device->SetTexture(0, texture);
                boundTexture = texture;
            }
            // Glyph UVs are in texels: the shader divides by the size of the atlas it samples.
            if (command.Texture == ColorGlyphAtlasTextureID || command.Texture == TextureID())
            {
                if (atlas != boundAtlas)
                {
                    output[2] = 1.0f / static_cast<float>(atlas->Width);
                    output[3] = 1.0f / static_cast<float>(atlas->Height);
                    device->SetPixelShaderConstantF(1, output, 1);
                    boundAtlas = atlas;
                }
            }

            const RECT scissor = {static_cast<LONG>(left), static_cast<LONG>(top), static_cast<LONG>(right),
                                  static_cast<LONG>(bottom)};
            device->SetScissorRect(&scissor);
            device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, vertexCount, command.IndexOffset,
                                         command.IndexCount / 3);
        }

        m_StateBlock->Apply();
        device->SetViewport(&savedViewport);
        device->SetScissorRect(&savedScissor);
    }

    void DX9Renderer::InvalidateDeviceObjects()
    {
        m_StateBlock.Reset();
        m_VertexBuffer.Reset();
        m_IndexBuffer.Reset();
        m_VertexCapacity = 0;
        m_IndexCapacity = 0;
        // The frame's geometry went with the buffers.
        m_HasNewDrawData = true;
        m_Atlas = AtlasTexture();
        m_ColorAtlas = AtlasTexture();
        InvalidateGlyphAtlas();

        // Host textures in D3DPOOL_DEFAULT must be released before a reset too; Carbon forgets them, and takes a
        // reference again when they are next drawn.
        std::vector<uint64_t> keys;
        keys.reserve(m_HostTextures.size());
        for (const auto& [key, texture] : m_HostTextures)
            keys.push_back(key);
        for (const uint64_t key : keys)
            ReleaseHostTexture(key);
        m_HostTextures.clear();
    }

    TextureID DX9Renderer::RegisterTexture(IDirect3DTexture9* texture)
    {
        if (texture == nullptr)
            return TextureID();
        const uint64_t key = MakeTextureID(texture).Value;
        ComPtr<IDirect3DTexture9>& hostTexture = m_HostTextures[key];
        if (hostTexture == nullptr)
            hostTexture = texture; // adds a reference
        return RegisterHostTexture(key);
    }

    void DX9Renderer::ReleaseTexture(TextureID texture)
    {
        m_HostTextures.erase(texture.Value);
    }
} // namespace Carbon::Internal
