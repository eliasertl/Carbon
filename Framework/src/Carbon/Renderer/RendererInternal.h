#pragma once

#include <cstdint>
#include <unordered_map>

#include <webgpu/webgpu_cpp.h>

#include "Carbon/Draw/DrawTypes.h"

namespace Carbon::Internal
{
    class GlyphAtlas;

    /// Turns draw data into Dawn calls. This is the only class in Carbon that talks to the GPU.
    ///
    /// It always exists, even for a headless context (null device); then it only keeps the texture registry and
    /// Render does nothing.
    class Renderer
    {
    public:
        Renderer(const wgpu::Device& device, wgpu::TextureFormat colorFormat, wgpu::TextureFormat depthStencilFormat,
                 uint32_t sampleCount);

        bool HasDevice() const { return m_Device != nullptr; }

        /// Uploads the atlas and the frame's buffers, then records the draw commands into `pass`.
        void Render(const DrawData& drawData, GlyphAtlas& atlas, const wgpu::RenderPassEncoder& pass);

        /// Registers a host texture for the current frame and returns its ID.
        TextureID RegisterTexture(const wgpu::TextureView& view);

        /// Releases host textures that were not drawn during the previous frame.
        void BeginFrame(uint64_t frameCount);

    private:
        struct HostTexture
        {
            wgpu::TextureView View;
            wgpu::BindGroup BindGroup;
            uint64_t LastUsedFrame = 0;
        };

        void CreatePipeline();
        bool EnsureBuffer(wgpu::Buffer& buffer, uint64_t& capacity, uint64_t requiredSize, wgpu::BufferUsage usage,
                          const char* label);
        void UploadAtlas(GlyphAtlas& atlas);
        wgpu::BindGroup CreateTextureBindGroup(const wgpu::TextureView& view) const;

    private:
        wgpu::Device m_Device;
        wgpu::Queue m_Queue;
        wgpu::TextureFormat m_ColorFormat;
        wgpu::TextureFormat m_DepthStencilFormat;
        uint32_t m_SampleCount;

        wgpu::RenderPipeline m_Pipeline;
        wgpu::BindGroupLayout m_FrameLayout;
        wgpu::BindGroupLayout m_TextureLayout;
        wgpu::Sampler m_Sampler;

        wgpu::Buffer m_FrameBuffer;
        wgpu::Buffer m_VertexBuffer;
        wgpu::Buffer m_IndexBuffer;
        wgpu::Buffer m_PrimitiveBuffer;
        uint64_t m_VertexCapacity = 0;
        uint64_t m_IndexCapacity = 0;
        uint64_t m_PrimitiveCapacity = 0;
        wgpu::BindGroup m_FrameBindGroup;

        wgpu::Texture m_AtlasTexture;
        wgpu::BindGroup m_AtlasBindGroup;
        uint32_t m_AtlasGeneration = 0;
        uint32_t m_AtlasWidth = 0;
        uint32_t m_AtlasHeight = 0;

        std::unordered_map<uint64_t, HostTexture> m_HostTextures;
        uint64_t m_FrameCount = 0;
        bool m_ReportedMissingDevice = false;
    };
} // namespace Carbon::Internal
