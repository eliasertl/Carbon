#include "Carbon/Backends/WebGPU/WebGPURendererInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"

namespace Carbon::Internal
{
    // Generated from Shaders/Carbon.wgsl by EmbedAsset.cmake.
    extern const unsigned char g_WebGPUShaderData[];
    extern const unsigned long long g_WebGPUShaderSize;

    namespace
    {
        // Mirrors `struct Frame` in Carbon.wgsl.
        struct FrameUniforms
        {
            float DisplayWidth;
            float DisplayHeight;
            float ContentScale;
            float LinearOutput;
        };

        constexpr uint64_t MinimumBufferSize = 16 * 1024;

        bool IsSrgbFormat(wgpu::TextureFormat format)
        {
            switch (format)
            {
                case wgpu::TextureFormat::RGBA8UnormSrgb:
                case wgpu::TextureFormat::BGRA8UnormSrgb:
                    return true;
                default:
                    return false;
            }
        }
    } // namespace

    WebGPURenderer::WebGPURenderer(const wgpu::Device& device, wgpu::TextureFormat colorFormat,
                                   wgpu::TextureFormat depthStencilFormat, uint32_t sampleCount)
        : m_Device(device),
          m_ColorFormat(colorFormat),
          m_DepthStencilFormat(depthStencilFormat),
          m_SampleCount(std::max<uint32_t>(sampleCount, 1))
    {
        m_Queue = m_Device.GetQueue();
        CreatePipeline();
    }

    void WebGPURenderer::CreatePipeline()
    {
        const std::string_view source(reinterpret_cast<const char*>(g_WebGPUShaderData),
                                      static_cast<size_t>(g_WebGPUShaderSize));
        wgpu::ShaderSourceWGSL wgsl;
        wgsl.code = wgpu::StringView(source.data(), source.size());
        wgpu::ShaderModuleDescriptor shaderDescriptor;
        shaderDescriptor.nextInChain = &wgsl;
        shaderDescriptor.label = "Carbon shader";
        const wgpu::ShaderModule shader = m_Device.CreateShaderModule(&shaderDescriptor);

        // Group 0: per-frame uniforms and the primitive array.
        std::array<wgpu::BindGroupLayoutEntry, 2> frameEntries;
        frameEntries[0].binding = 0;
        frameEntries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
        frameEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
        frameEntries[0].buffer.minBindingSize = sizeof(FrameUniforms);
        frameEntries[1].binding = 1;
        frameEntries[1].visibility = wgpu::ShaderStage::Fragment;
        frameEntries[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
        frameEntries[1].buffer.minBindingSize = sizeof(DrawPrimitive);
        wgpu::BindGroupLayoutDescriptor frameLayoutDescriptor;
        frameLayoutDescriptor.label = "Carbon frame layout";
        frameLayoutDescriptor.entryCount = frameEntries.size();
        frameLayoutDescriptor.entries = frameEntries.data();
        m_FrameLayout = m_Device.CreateBindGroupLayout(&frameLayoutDescriptor);

        // Group 1: the texture of a draw command (the glyph atlas or a host texture).
        std::array<wgpu::BindGroupLayoutEntry, 2> textureEntries;
        textureEntries[0].binding = 0;
        textureEntries[0].visibility = wgpu::ShaderStage::Fragment;
        textureEntries[0].texture.sampleType = wgpu::TextureSampleType::Float;
        textureEntries[0].texture.viewDimension = wgpu::TextureViewDimension::e2D;
        textureEntries[1].binding = 1;
        textureEntries[1].visibility = wgpu::ShaderStage::Fragment;
        textureEntries[1].sampler.type = wgpu::SamplerBindingType::Filtering;
        wgpu::BindGroupLayoutDescriptor textureLayoutDescriptor;
        textureLayoutDescriptor.label = "Carbon texture layout";
        textureLayoutDescriptor.entryCount = textureEntries.size();
        textureLayoutDescriptor.entries = textureEntries.data();
        m_TextureLayout = m_Device.CreateBindGroupLayout(&textureLayoutDescriptor);

        const std::array<wgpu::BindGroupLayout, 2> layouts = {m_FrameLayout, m_TextureLayout};
        wgpu::PipelineLayoutDescriptor layoutDescriptor;
        layoutDescriptor.label = "Carbon pipeline layout";
        layoutDescriptor.bindGroupLayoutCount = layouts.size();
        layoutDescriptor.bindGroupLayouts = layouts.data();
        const wgpu::PipelineLayout pipelineLayout = m_Device.CreatePipelineLayout(&layoutDescriptor);

        // Vertex layout: mirrors DrawVertex.
        static_assert(sizeof(DrawVertex) == 32, "DrawVertex must match the vertex layout below");
        static_assert(sizeof(DrawPrimitive) == 32, "DrawPrimitive must match `struct Primitive` in Carbon.wgsl");
        std::array<wgpu::VertexAttribute, 5> attributes;
        attributes[0].format = wgpu::VertexFormat::Float32x2;
        attributes[0].offset = offsetof(DrawVertex, Position);
        attributes[0].shaderLocation = 0;
        attributes[1].format = wgpu::VertexFormat::Float32x2;
        attributes[1].offset = offsetof(DrawVertex, Local);
        attributes[1].shaderLocation = 1;
        attributes[2].format = wgpu::VertexFormat::Float32x2;
        attributes[2].offset = offsetof(DrawVertex, UV);
        attributes[2].shaderLocation = 2;
        attributes[3].format = wgpu::VertexFormat::Unorm8x4;
        attributes[3].offset = offsetof(DrawVertex, Color);
        attributes[3].shaderLocation = 3;
        attributes[4].format = wgpu::VertexFormat::Uint32;
        attributes[4].offset = offsetof(DrawVertex, Primitive);
        attributes[4].shaderLocation = 4;
        wgpu::VertexBufferLayout vertexLayout;
        vertexLayout.arrayStride = sizeof(DrawVertex);
        vertexLayout.stepMode = wgpu::VertexStepMode::Vertex;
        vertexLayout.attributeCount = attributes.size();
        vertexLayout.attributes = attributes.data();

        // Premultiplied alpha blending.
        wgpu::BlendState blend;
        blend.color.operation = wgpu::BlendOperation::Add;
        blend.color.srcFactor = wgpu::BlendFactor::One;
        blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blend.alpha = blend.color;
        wgpu::ColorTargetState colorTarget;
        colorTarget.format = m_ColorFormat;
        colorTarget.blend = &blend;
        wgpu::FragmentState fragment;
        fragment.module = shader;
        fragment.entryPoint = "FragmentMain";
        fragment.targetCount = 1;
        fragment.targets = &colorTarget;

        // Carbon neither tests nor writes depth, but the pipeline must name the pass's depth-stencil format.
        wgpu::DepthStencilState depthStencil;
        depthStencil.format = m_DepthStencilFormat;
        depthStencil.depthWriteEnabled = wgpu::OptionalBool::False;
        depthStencil.depthCompare = wgpu::CompareFunction::Always;

        wgpu::RenderPipelineDescriptor descriptor;
        descriptor.label = "Carbon pipeline";
        descriptor.layout = pipelineLayout;
        descriptor.vertex.module = shader;
        descriptor.vertex.entryPoint = "VertexMain";
        descriptor.vertex.bufferCount = 1;
        descriptor.vertex.buffers = &vertexLayout;
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        descriptor.primitive.cullMode = wgpu::CullMode::None;
        descriptor.fragment = &fragment;
        descriptor.multisample.count = m_SampleCount;
        if (m_DepthStencilFormat != wgpu::TextureFormat::Undefined)
            descriptor.depthStencil = &depthStencil;
        m_Pipeline = m_Device.CreateRenderPipeline(&descriptor);

        wgpu::SamplerDescriptor samplerDescriptor;
        samplerDescriptor.label = "Carbon sampler";
        samplerDescriptor.magFilter = wgpu::FilterMode::Linear;
        samplerDescriptor.minFilter = wgpu::FilterMode::Linear;
        samplerDescriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
        samplerDescriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
        m_Sampler = m_Device.CreateSampler(&samplerDescriptor);

        wgpu::BufferDescriptor frameBufferDescriptor;
        frameBufferDescriptor.label = "Carbon frame uniforms";
        frameBufferDescriptor.size = sizeof(FrameUniforms);
        frameBufferDescriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        m_FrameBuffer = m_Device.CreateBuffer(&frameBufferDescriptor);
    }

    bool WebGPURenderer::EnsureBuffer(wgpu::Buffer& buffer, uint64_t& capacity, uint64_t requiredSize,
                                      wgpu::BufferUsage usage, const char* label)
    {
        if (buffer != nullptr && capacity >= requiredSize)
            return false;

        // Grow geometrically so steady-state frames never reallocate.
        uint64_t newCapacity = std::max(capacity * 2, MinimumBufferSize);
        while (newCapacity < requiredSize)
            newCapacity *= 2;

        wgpu::BufferDescriptor descriptor;
        descriptor.label = label;
        descriptor.size = newCapacity;
        descriptor.usage = usage | wgpu::BufferUsage::CopyDst;
        buffer = m_Device.CreateBuffer(&descriptor);
        capacity = newCapacity;
        return true;
    }

    void WebGPURenderer::UpdateGlyphAtlas(const GlyphAtlasUpdate& update)
    {
        // A full update may come with a new size; the changed rows of a partial one fit the texture there is.
        if (m_AtlasTexture == nullptr || m_AtlasWidth != update.Width || m_AtlasHeight != update.Height)
        {
            wgpu::TextureDescriptor descriptor;
            descriptor.label = "Carbon glyph atlas";
            descriptor.size = {update.Width, update.Height, 1};
            descriptor.format = wgpu::TextureFormat::R8Unorm;
            descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
            m_AtlasTexture = m_Device.CreateTexture(&descriptor);
            m_AtlasBindGroup = CreateTextureBindGroup(m_AtlasTexture.CreateView());
            m_AtlasWidth = update.Width;
            m_AtlasHeight = update.Height;
        }
        if (update.RowCount == 0)
            return;

        wgpu::TexelCopyTextureInfo destination;
        destination.texture = m_AtlasTexture;
        destination.origin = {0, update.FirstRow, 0};
        wgpu::TexelCopyBufferLayout layout;
        layout.bytesPerRow = update.Width;
        layout.rowsPerImage = update.RowCount;
        const wgpu::Extent3D extent = {update.Width, update.RowCount, 1};
        const uint8_t* pixels = update.Pixels.data() + static_cast<size_t>(update.FirstRow) * update.Width;
        m_Queue.WriteTexture(&destination, pixels, static_cast<size_t>(extent.width) * extent.height, &layout, &extent);
    }

    wgpu::BindGroup WebGPURenderer::CreateTextureBindGroup(const wgpu::TextureView& view) const
    {
        std::array<wgpu::BindGroupEntry, 2> entries;
        entries[0].binding = 0;
        entries[0].textureView = view;
        entries[1].binding = 1;
        entries[1].sampler = m_Sampler;
        wgpu::BindGroupDescriptor descriptor;
        descriptor.layout = m_TextureLayout;
        descriptor.entryCount = entries.size();
        descriptor.entries = entries.data();
        return m_Device.CreateBindGroup(&descriptor);
    }

    void WebGPURenderer::Render(const DrawData& drawData)
    {
        CB_VERIFY(m_Pass != nullptr, "The WebGPU renderer has no render pass to draw into");
        if (m_Pass == nullptr)
            return;
        const wgpu::RenderPassEncoder& pass = m_Pass;

        const float scale = drawData.ContentScale;
        const uint32_t targetWidth = static_cast<uint32_t>(std::lround(drawData.DisplaySize.X * scale));
        const uint32_t targetHeight = static_cast<uint32_t>(std::lround(drawData.DisplaySize.Y * scale));

        const uint64_t vertexBytes = drawData.Vertices.size_bytes();
        const uint64_t indexBytes = drawData.Indices.size_bytes();
        const uint64_t primitiveBytes = std::max<uint64_t>(drawData.Primitives.size_bytes(), sizeof(DrawPrimitive));
        const bool vertexBufferChanged =
            EnsureBuffer(m_VertexBuffer, m_VertexCapacity, vertexBytes, wgpu::BufferUsage::Vertex, "Carbon vertices");
        const bool indexBufferChanged =
            EnsureBuffer(m_IndexBuffer, m_IndexCapacity, indexBytes, wgpu::BufferUsage::Index, "Carbon indices");
        const bool primitiveBufferChanged = EnsureBuffer(m_PrimitiveBuffer, m_PrimitiveCapacity, primitiveBytes,
                                                         wgpu::BufferUsage::Storage, "Carbon primitives");
        if (primitiveBufferChanged || m_FrameBindGroup == nullptr)
        {
            std::array<wgpu::BindGroupEntry, 2> entries;
            entries[0].binding = 0;
            entries[0].buffer = m_FrameBuffer;
            entries[0].size = sizeof(FrameUniforms);
            entries[1].binding = 1;
            entries[1].buffer = m_PrimitiveBuffer;
            entries[1].size = m_PrimitiveCapacity;
            wgpu::BindGroupDescriptor descriptor;
            descriptor.layout = m_FrameLayout;
            descriptor.entryCount = entries.size();
            descriptor.entries = entries.data();
            m_FrameBindGroup = m_Device.CreateBindGroup(&descriptor);
        }

        // The frame's data is uploaded once, by the first Render after EndFrame.
        if (m_HasNewDrawData || vertexBufferChanged || indexBufferChanged || primitiveBufferChanged)
        {
            FrameUniforms uniforms;
            uniforms.DisplayWidth = drawData.DisplaySize.X;
            uniforms.DisplayHeight = drawData.DisplaySize.Y;
            uniforms.ContentScale = scale;
            uniforms.LinearOutput = IsSrgbFormat(m_ColorFormat) ? 1.0f : 0.0f;
            m_Queue.WriteBuffer(m_FrameBuffer, 0, &uniforms, sizeof(uniforms));
            m_Queue.WriteBuffer(m_VertexBuffer, 0, drawData.Vertices.data(), vertexBytes);
            m_Queue.WriteBuffer(m_IndexBuffer, 0, drawData.Indices.data(), indexBytes);
            if (!drawData.Primitives.empty())
            {
                m_Queue.WriteBuffer(m_PrimitiveBuffer, 0, drawData.Primitives.data(), drawData.Primitives.size_bytes());
            }
            m_HasNewDrawData = false;
        }

        pass.SetPipeline(m_Pipeline);
        pass.SetViewport(0.0f, 0.0f, static_cast<float>(targetWidth), static_cast<float>(targetHeight), 0.0f, 1.0f);
        pass.SetBindGroup(0, m_FrameBindGroup);
        pass.SetVertexBuffer(0, m_VertexBuffer, 0, vertexBytes);
        pass.SetIndexBuffer(m_IndexBuffer, wgpu::IndexFormat::Uint32, 0, indexBytes);

        for (const DrawCommand& command : drawData.Commands)
        {
            // Clip rectangles are in points; the scissor rectangle is in pixels and must stay inside the target.
            const float left = std::clamp(std::floor(command.ClipRect.X * scale + 0.5f), 0.0f, float(targetWidth));
            const float top = std::clamp(std::floor(command.ClipRect.Y * scale + 0.5f), 0.0f, float(targetHeight));
            const float right =
                std::clamp(std::floor(command.ClipRect.GetRight() * scale + 0.5f), 0.0f, float(targetWidth));
            const float bottom =
                std::clamp(std::floor(command.ClipRect.GetBottom() * scale + 0.5f), 0.0f, float(targetHeight));
            if (right <= left || bottom <= top)
                continue;

            if (command.Texture == TextureID())
            {
                pass.SetBindGroup(1, m_AtlasBindGroup);
            }
            else
            {
                // A texture not registered with WebGPUGetTextureID is a raw WGPUTextureView (MakeTextureID). The
                // wrapper takes a reference of its own, so the view lives as long as Carbon uses it.
                HostTexture& texture = m_HostTextures[command.Texture.Value];
                if (texture.View == nullptr)
                    texture.View = wgpu::TextureView(reinterpret_cast<WGPUTextureView>(command.Texture.Value));
                if (texture.BindGroup == nullptr)
                    texture.BindGroup = CreateTextureBindGroup(texture.View);
                pass.SetBindGroup(1, texture.BindGroup);
            }

            pass.SetScissorRect(static_cast<uint32_t>(left), static_cast<uint32_t>(top),
                                static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top));
            pass.DrawIndexed(command.IndexCount, 1, command.IndexOffset, 0, 0);
        }
    }

    TextureID WebGPURenderer::RegisterTexture(const wgpu::TextureView& view)
    {
        if (view == nullptr)
            return TextureID();
        // The view's handle is unique while Carbon holds a reference to it. Carbon's core decides how long the
        // texture stays registered and calls ReleaseTexture when it is no longer used.
        const uint64_t key = MakeTextureID(view.Get()).Value;
        HostTexture& texture = m_HostTextures[key];
        if (texture.View == nullptr)
            texture.View = view;
        return RegisterHostTexture(key);
    }

    void WebGPURenderer::ReleaseTexture(TextureID texture)
    {
        m_HostTextures.erase(texture.Value);
    }

    RendererBackendCapabilities WebGPURenderer::GetCapabilities() const
    {
        RendererBackendCapabilities capabilities;
        wgpu::Limits limits;
        if (m_Device.GetLimits(&limits) == wgpu::Status::Success)
            capabilities.MaxTextureSize = limits.maxTextureDimension2D;
        return capabilities;
    }
} // namespace Carbon::Internal
