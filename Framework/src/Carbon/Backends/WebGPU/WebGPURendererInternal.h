#pragma once

#include <cstdint>
#include <unordered_map>

#include <webgpu/webgpu_cpp.h>

#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon::Internal
{
    /// The WebGPU renderer backend: turns draw data into Dawn calls.
    class WebGPURenderer : public RendererBackend
    {
    public:
        WebGPURenderer(const wgpu::Device& device, wgpu::TextureFormat colorFormat,
                       wgpu::TextureFormat depthStencilFormat, uint32_t sampleCount);

        std::string_view GetName() const override { return "WebGPU"; }
        RendererBackendCapabilities GetCapabilities() const override;
        void EndFrame() override { m_HasNewDrawData = true; }
        void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) override;
        void Render(const DrawData& drawData) override;
        void ReleaseTexture(TextureID texture) override;

        /// The pass the next Render call records into; null afterwards.
        void SetRenderPass(const wgpu::RenderPassEncoder& pass) { m_Pass = pass; }

        /// Registers a host texture for the current frame and returns its ID.
        TextureID RegisterTexture(const wgpu::TextureView& view);

    private:
        struct HostTexture
        {
            wgpu::TextureView View;
            wgpu::BindGroup BindGroup;
        };

        void CreatePipeline();
        bool EnsureBuffer(wgpu::Buffer& buffer, uint64_t& capacity, uint64_t requiredSize, wgpu::BufferUsage usage,
                          const char* label);
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
        /// Set by EndFrame: the next Render has new draw data to upload. A frame that is rendered again (a window
        /// redrawn without a new frame) reuses what its first Render uploaded.
        bool m_HasNewDrawData = true;
        uint64_t m_VertexCapacity = 0;
        uint64_t m_IndexCapacity = 0;
        uint64_t m_PrimitiveCapacity = 0;
        wgpu::BindGroup m_FrameBindGroup;

        wgpu::Texture m_AtlasTexture;
        wgpu::BindGroup m_AtlasBindGroup;
        uint32_t m_AtlasWidth = 0;
        uint32_t m_AtlasHeight = 0;

        wgpu::RenderPassEncoder m_Pass;
        std::unordered_map<uint64_t, HostTexture> m_HostTextures;
    };
} // namespace Carbon::Internal
