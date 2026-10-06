#include "Carbon/Backends/Vulkan/VulkanAllocatorInternal.h"

#include <algorithm>

#include "Carbon/Core/Log.h"

namespace Carbon::Internal
{
    namespace
    {
        constexpr VkDeviceSize BlockSize = 4 * 1024 * 1024;

        VkDeviceSize AlignUp(VkDeviceSize value, VkDeviceSize alignment)
        {
            return alignment <= 1 ? value : (value + alignment - 1) / alignment * alignment;
        }
    } // namespace

    VulkanAllocator::VulkanAllocator(VkPhysicalDevice physicalDevice, VkDevice device,
                                     const VkAllocationCallbacks* callbacks)
        : m_Device(device), m_Callbacks(callbacks)
    {
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_Properties);
    }

    VulkanAllocator::~VulkanAllocator()
    {
        for (Block& block : m_Blocks)
            DestroyBlock(block);
    }

    bool VulkanAllocator::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties,
                                         uint32_t& memoryType) const
    {
        for (uint32_t i = 0; i < m_Properties.memoryTypeCount; i++)
        {
            if ((typeBits & (1u << i)) != 0 && (m_Properties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                memoryType = i;
                return true;
            }
        }
        return false;
    }

    bool VulkanAllocator::CreateBlock(uint32_t memoryType, VkDeviceSize size, bool isImage, bool isDedicated,
                                      uint32_t& index)
    {
        VkMemoryAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = size;
        allocateInfo.memoryTypeIndex = memoryType;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        const VkResult result = vkAllocateMemory(m_Device, &allocateInfo, m_Callbacks, &memory);
        if (result != VK_SUCCESS)
        {
            CB_LOG_ERROR("Vulkan", "vkAllocateMemory of {} bytes failed ({})", size, static_cast<int>(result));
            return false;
        }

        Block block;
        block.Memory = memory;
        block.Size = size;
        block.MemoryType = memoryType;
        block.IsImage = isImage;
        block.IsDedicated = isDedicated;
        block.FreeRanges.push_back(Range{0, size});
        if ((m_Properties.memoryTypes[memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
        {
            if (vkMapMemory(m_Device, memory, 0, VK_WHOLE_SIZE, 0, &block.Mapped) != VK_SUCCESS)
            {
                vkFreeMemory(m_Device, memory, m_Callbacks);
                CB_LOG_ERROR("Vulkan", "vkMapMemory failed");
                return false;
            }
        }

        // Reuse the slot of a destroyed block.
        for (uint32_t i = 0; i < m_Blocks.size(); i++)
        {
            if (m_Blocks[i].Memory == VK_NULL_HANDLE)
            {
                m_Blocks[i] = std::move(block);
                index = i;
                return true;
            }
        }
        m_Blocks.push_back(std::move(block));
        index = static_cast<uint32_t>(m_Blocks.size() - 1);
        return true;
    }

    bool VulkanAllocator::TakeRange(Block& block, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize& offset)
    {
        for (size_t i = 0; i < block.FreeRanges.size(); i++)
        {
            const Range range = block.FreeRanges[i];
            const VkDeviceSize start = AlignUp(range.Offset, alignment);
            if (start + size > range.Offset + range.Size)
                continue;

            // Split the range into what lies before and after the allocation.
            const Range before{range.Offset, start - range.Offset};
            const Range after{start + size, range.Offset + range.Size - (start + size)};
            block.FreeRanges.erase(block.FreeRanges.begin() + static_cast<ptrdiff_t>(i));
            if (after.Size > 0)
                block.FreeRanges.insert(block.FreeRanges.begin() + static_cast<ptrdiff_t>(i), after);
            if (before.Size > 0)
                block.FreeRanges.insert(block.FreeRanges.begin() + static_cast<ptrdiff_t>(i), before);
            offset = start;
            return true;
        }
        return false;
    }

    bool VulkanAllocator::Allocate(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties,
                                   bool isImage, VulkanAllocation& allocation)
    {
        allocation = VulkanAllocation();
        uint32_t memoryType = 0;
        if (!FindMemoryType(requirements.memoryTypeBits, properties, memoryType))
        {
            CB_LOG_ERROR("Vulkan", "No memory type with properties {:#x} for this resource",
                         static_cast<uint32_t>(properties));
            return false;
        }

        const bool isDedicated = requirements.size > BlockSize / 2;
        uint32_t index = 0;
        VkDeviceSize offset = 0;
        bool found = false;
        if (!isDedicated)
        {
            for (uint32_t i = 0; i < m_Blocks.size() && !found; i++)
            {
                Block& block = m_Blocks[i];
                if (block.Memory == VK_NULL_HANDLE || block.IsDedicated || block.MemoryType != memoryType ||
                    block.IsImage != isImage)
                    continue;
                if (TakeRange(block, requirements.size, requirements.alignment, offset))
                {
                    index = i;
                    found = true;
                }
            }
        }
        if (!found)
        {
            const VkDeviceSize size = isDedicated ? requirements.size : BlockSize;
            if (!CreateBlock(memoryType, size, isImage, isDedicated, index))
                return false;
            if (!TakeRange(m_Blocks[index], requirements.size, requirements.alignment, offset))
                return false;
        }

        Block& block = m_Blocks[index];
        block.AllocationCount++;
        allocation.Memory = block.Memory;
        allocation.Offset = offset;
        allocation.Size = requirements.size;
        allocation.Mapped = block.Mapped != nullptr ? static_cast<uint8_t*>(block.Mapped) + offset : nullptr;
        allocation.Block = index;
        return true;
    }

    void VulkanAllocator::Free(const VulkanAllocation& allocation)
    {
        if (allocation.Memory == VK_NULL_HANDLE || allocation.Block >= m_Blocks.size())
            return;
        Block& block = m_Blocks[allocation.Block];
        if (block.Memory != allocation.Memory)
            return;

        block.AllocationCount--;
        if (block.IsDedicated || block.AllocationCount == 0)
        {
            // An empty block is given back to the driver; the next allocation creates a fresh one.
            DestroyBlock(block);
            return;
        }

        // Insert the range in offset order and merge it with its neighbours.
        Range range{allocation.Offset, allocation.Size};
        const auto position =
            std::lower_bound(block.FreeRanges.begin(), block.FreeRanges.end(), range.Offset,
                             [](const Range& existing, VkDeviceSize value) { return existing.Offset < value; });
        size_t i = static_cast<size_t>(position - block.FreeRanges.begin());
        block.FreeRanges.insert(position, range);
        if (i + 1 < block.FreeRanges.size() &&
            block.FreeRanges[i].Offset + block.FreeRanges[i].Size == block.FreeRanges[i + 1].Offset)
        {
            block.FreeRanges[i].Size += block.FreeRanges[i + 1].Size;
            block.FreeRanges.erase(block.FreeRanges.begin() + static_cast<ptrdiff_t>(i + 1));
        }
        if (i > 0 && block.FreeRanges[i - 1].Offset + block.FreeRanges[i - 1].Size == block.FreeRanges[i].Offset)
        {
            block.FreeRanges[i - 1].Size += block.FreeRanges[i].Size;
            block.FreeRanges.erase(block.FreeRanges.begin() + static_cast<ptrdiff_t>(i));
        }
    }

    void VulkanAllocator::DestroyBlock(Block& block)
    {
        if (block.Memory == VK_NULL_HANDLE)
            return;
        if (block.Mapped != nullptr)
            vkUnmapMemory(m_Device, block.Memory);
        vkFreeMemory(m_Device, block.Memory, m_Callbacks);
        block = Block();
    }
} // namespace Carbon::Internal
