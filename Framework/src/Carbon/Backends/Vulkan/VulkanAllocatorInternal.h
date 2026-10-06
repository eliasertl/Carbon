#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace Carbon::Internal
{
    /// A piece of device memory handed out by VulkanAllocator.
    struct VulkanAllocation
    {
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        VkDeviceSize Offset = 0;
        VkDeviceSize Size = 0;
        /// Where the allocation is mapped, for host-visible memory; null otherwise.
        void* Mapped = nullptr;
        /// Index of the block it came from.
        uint32_t Block = 0;
    };

    /// The Vulkan backend's memory allocator: a few large blocks per memory type, carved up with a first-fit free
    /// list. Carbon allocates rarely (buffers that grow, the glyph atlas, staging memory), so this is all it needs.
    ///
    /// Buffers and optimally tiled images never share a block, which sidesteps bufferImageGranularity. Requests
    /// larger than half a block get a block of their own, which is freed with them. Host-visible blocks stay
    /// mapped for their whole life.
    class VulkanAllocator
    {
    public:
        VulkanAllocator(VkPhysicalDevice physicalDevice, VkDevice device, const VkAllocationCallbacks* callbacks);
        ~VulkanAllocator();

        VulkanAllocator(const VulkanAllocator&) = delete;
        VulkanAllocator& operator=(const VulkanAllocator&) = delete;

        /// Allocates memory for `requirements` from a memory type with `properties`. `isImage` keeps optimally
        /// tiled images apart from buffers. Returns false when no memory type fits or the device is out of memory.
        bool Allocate(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties, bool isImage,
                      VulkanAllocation& allocation);

        /// Returns an allocation; a null allocation is ignored.
        void Free(const VulkanAllocation& allocation);

    private:
        struct Range
        {
            VkDeviceSize Offset = 0;
            VkDeviceSize Size = 0;
        };

        struct Block
        {
            VkDeviceMemory Memory = VK_NULL_HANDLE;
            VkDeviceSize Size = 0;
            void* Mapped = nullptr;
            uint32_t MemoryType = 0;
            bool IsImage = false;
            bool IsDedicated = false;
            uint32_t AllocationCount = 0;
            /// Free ranges, sorted by offset and never adjacent.
            std::vector<Range> FreeRanges;
        };

        bool FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t& memoryType) const;
        bool CreateBlock(uint32_t memoryType, VkDeviceSize size, bool isImage, bool isDedicated, uint32_t& index);
        static bool TakeRange(Block& block, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize& offset);
        void DestroyBlock(Block& block);

    private:
        VkDevice m_Device;
        const VkAllocationCallbacks* m_Callbacks;
        VkPhysicalDeviceMemoryProperties m_Properties = {};
        /// Destroyed blocks keep their slot (Memory null), so allocation indices stay valid.
        std::vector<Block> m_Blocks;
    };
} // namespace Carbon::Internal
