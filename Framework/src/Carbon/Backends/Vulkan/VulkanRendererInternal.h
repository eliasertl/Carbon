#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "Carbon/Backends/Vulkan/VulkanAllocatorInternal.h"
#include "Carbon/Backends/Vulkan/VulkanBackend.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon::Internal
{
    /// The Vulkan renderer backend.
    ///
    /// Frames in flight: each frame's vertices, indices and primitives go into the buffers of one of
    /// FramesInFlight slots, which rotate once per frame (EndFrame), so Carbon never writes a buffer the GPU may
    /// still read. Objects that a recorded frame may still use (a replaced atlas image, a released texture's
    /// descriptor set, a buffer that grew) are destroyed FramesInFlight frames later.
    ///
    /// Glyph-atlas uploads are recorded into command buffers of Carbon's own and submitted to the host's queue at
    /// once, ahead of the host's submission of the frame. Barriers in them order the copy after earlier frames'
    /// sampling and before later frames' sampling.
    class VulkanRenderer : public RendererBackend
    {
    public:
        VulkanRenderer(const VulkanInitInfo& info, VkFormat colorFormat, VkFormat depthFormat, VkFormat stencilFormat);
        ~VulkanRenderer() override;

        /// False when creating the pipeline or another object failed; the reason was logged.
        bool IsValid() const { return m_IsValid; }

        std::string_view GetName() const override { return "Vulkan"; }
        RendererBackendCapabilities GetCapabilities() const override;
        void EndFrame() override;
        void UpdateGlyphAtlas(const GlyphAtlasUpdate& update) override;
        void Render(const DrawData& drawData) override;
        void ReleaseTexture(TextureID texture) override;

        /// The command buffer the next Render call records into; null afterwards.
        void SetCommandBuffer(VkCommandBuffer commandBuffer) { m_CommandBuffer = commandBuffer; }

        /// Registers a host image view for the current frame and returns its ID.
        TextureID RegisterTexture(VkImageView view, VkImageLayout layout);

        /// The key a view is registered under: its handle, which is a pointer or an integer depending on the
        /// platform.
        static uint64_t GetTextureKey(VkImageView view) { return MakeTextureID(view).Value; }

    private:
        /// The view a key stands for; the inverse of GetTextureKey.
        static VkImageView ToView(uint64_t key) { return FromKey<VkImageView>(key); }

        template <typename Handle>
        static Handle FromKey(uint64_t key)
        {
            if constexpr (std::is_pointer_v<Handle>)
                return reinterpret_cast<Handle>(static_cast<uintptr_t>(key));
            else
                return static_cast<Handle>(key);
        }

    private:
        struct Buffer
        {
            VkBuffer Handle = VK_NULL_HANDLE;
            VulkanAllocation Memory;
            VkDeviceSize Capacity = 0;
        };

        /// The per-frame buffers of one frame in flight.
        struct FrameSlot
        {
            Buffer Vertices;
            Buffer Indices;
            Buffer Primitives;
            /// Set 0: the primitive buffer.
            VkDescriptorSet PrimitiveSet = VK_NULL_HANDLE;
        };

        /// A command buffer and staging buffer for one glyph-atlas upload, reused once its fence signalled.
        struct Upload
        {
            VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
            VkFence Fence = VK_NULL_HANDLE;
            Buffer Staging;
            bool IsPending = false;
        };

        struct HostTexture
        {
            VkImageView View = VK_NULL_HANDLE;
            VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkDescriptorSet Set = VK_NULL_HANDLE;
        };

        /// One of the glyph atlases: R8_UNORM for coverage, R8G8B8A8_UNORM for color glyphs.
        struct AtlasImage
        {
            VkImage Image = VK_NULL_HANDLE;
            VkImageView View = VK_NULL_HANDLE;
            VulkanAllocation Memory;
            VkDescriptorSet Set = VK_NULL_HANDLE;
            uint32_t Width = 0;
            uint32_t Height = 0;
            /// The image has received a full upload; before that its contents are undefined.
            bool IsInitialized = false;
        };

        /// Something a frame in flight may still use, destroyed once FrameNumber + FramesInFlight is reached.
        struct Retired
        {
            uint64_t FrameNumber = 0;
            VkBuffer Buffer = VK_NULL_HANDLE;
            VkImage Image = VK_NULL_HANDLE;
            VkImageView View = VK_NULL_HANDLE;
            VkDescriptorSet Set = VK_NULL_HANDLE;
            VulkanAllocation Memory;
        };

        bool CreateObjects(VkFormat colorFormat, VkFormat depthFormat, VkFormat stencilFormat);
        bool CreatePipeline(VkFormat colorFormat, VkFormat depthFormat, VkFormat stencilFormat);
        bool CreateBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage);
        bool EnsureBuffer(Buffer& buffer, VkDeviceSize requiredSize, VkBufferUsageFlags usage);
        bool CreateAtlasImage(AtlasImage& atlas, VkFormat format, uint32_t width, uint32_t height);
        void DestroyAtlasImage(AtlasImage& atlas);
        VkDescriptorSet AllocateTextureSet(VkImageView view, VkImageLayout layout);
        void Retire(Retired retired);
        void RetireBuffer(Buffer& buffer);
        void DestroyRetired(bool all);
        void DestroyBuffer(Buffer& buffer);

    private:
        VulkanInitInfo m_Info;
        bool m_IsValid = false;
        bool m_OwnsDescriptorPool = false;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        std::unique_ptr<VulkanAllocator> m_Allocator;

        VkDescriptorSetLayout m_PrimitiveLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_TextureLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
        VkSampler m_Sampler = VK_NULL_HANDLE;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        bool m_IsLinearOutput = false;

        std::vector<FrameSlot> m_Slots;
        std::vector<Upload> m_Uploads;
        size_t m_NextUpload = 0;
        /// Counts frames whose draw data was uploaded; the current frame's slot is FrameNumber % FramesInFlight.
        uint64_t m_FrameNumber = 0;
        /// Set by EndFrame: the next Render has new draw data to upload.
        bool m_HasNewDrawData = false;
        bool m_HasUploadedFrame = false;

        AtlasImage m_Atlas;
        AtlasImage m_ColorAtlas;

        std::unordered_map<uint64_t, HostTexture> m_HostTextures;
        std::vector<Retired> m_Retired;
        VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
    };
} // namespace Carbon::Internal
