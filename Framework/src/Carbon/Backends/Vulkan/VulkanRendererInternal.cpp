#include "Carbon/Backends/Vulkan/VulkanRendererInternal.h"

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
        // SPIR-V generated at build time by glslc from Shaders/Carbon.vert and Shaders/Carbon.frag.
        const uint32_t VertexShaderCode[] = {
#include "Carbon/Backends/Vulkan/Carbon.vert.inc"
        };
        const uint32_t FragmentShaderCode[] = {
#include "Carbon/Backends/Vulkan/Carbon.frag.inc"
        };

        // Mirrors the push-constant block `Frame` in the shaders.
        struct FrameConstants
        {
            float DisplayWidth;
            float DisplayHeight;
            float ContentScale;
            float LinearOutput;
        };

        constexpr VkDeviceSize MinimumBufferSize = 16 * 1024;
        // Descriptor sets in Carbon's own pool: one per host texture in use, plus the atlas and the frames.
        constexpr uint32_t PoolTextureSets = 1024;

        constexpr VkMemoryPropertyFlags HostMemory =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        bool Check(VkResult result, const char* what)
        {
            if (result == VK_SUCCESS)
                return true;
            CB_LOG_ERROR("Vulkan", "{} failed ({})", what, static_cast<int>(result));
            return false;
        }

        VkShaderModule CreateShaderModule(VkDevice device, const uint32_t* code, size_t size,
                                          const VkAllocationCallbacks* callbacks)
        {
            VkShaderModuleCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = size;
            createInfo.pCode = code;
            VkShaderModule module = VK_NULL_HANDLE;
            Check(vkCreateShaderModule(device, &createInfo, callbacks, &module), "vkCreateShaderModule");
            return module;
        }
    } // namespace

    VulkanRenderer::VulkanRenderer(const VulkanInitInfo& info, VkFormat colorFormat, VkFormat depthFormat,
                                   VkFormat stencilFormat)
        : m_Info(info)
    {
        m_Info.FramesInFlight = std::max<uint32_t>(m_Info.FramesInFlight, 1);
        m_Info.SampleCount = std::max<uint32_t>(m_Info.SampleCount, 1);
        m_IsLinearOutput = IsSrgbFormat(info.ColorFormat);
        m_Allocator = std::make_unique<VulkanAllocator>(info.PhysicalDevice, info.Device, info.Allocator);
        m_IsValid = CreateObjects(colorFormat, depthFormat, stencilFormat);
    }

    VulkanRenderer::~VulkanRenderer()
    {
        const VkDevice device = m_Info.Device;
        const VkAllocationCallbacks* callbacks = m_Info.Allocator;

        // The host made sure its frames are done; Carbon's own uploads may still be running.
        for (Upload& upload : m_Uploads)
        {
            if (upload.IsPending)
                vkWaitForFences(device, 1, &upload.Fence, VK_TRUE, UINT64_MAX);
            if (upload.Fence != VK_NULL_HANDLE)
                vkDestroyFence(device, upload.Fence, callbacks);
            DestroyBuffer(upload.Staging);
        }
        DestroyRetired(true);

        for (auto& [key, texture] : m_HostTextures)
        {
            if (texture.Set != VK_NULL_HANDLE)
                vkFreeDescriptorSets(device, m_DescriptorPool, 1, &texture.Set);
        }
        for (FrameSlot& slot : m_Slots)
        {
            DestroyBuffer(slot.Vertices);
            DestroyBuffer(slot.Indices);
            DestroyBuffer(slot.Primitives);
            if (slot.PrimitiveSet != VK_NULL_HANDLE)
                vkFreeDescriptorSets(device, m_DescriptorPool, 1, &slot.PrimitiveSet);
        }
        if (m_AtlasSet != VK_NULL_HANDLE)
            vkFreeDescriptorSets(device, m_DescriptorPool, 1, &m_AtlasSet);
        if (m_AtlasView != VK_NULL_HANDLE)
            vkDestroyImageView(device, m_AtlasView, callbacks);
        if (m_AtlasImage != VK_NULL_HANDLE)
            vkDestroyImage(device, m_AtlasImage, callbacks);
        m_Allocator->Free(m_AtlasMemory);

        if (m_CommandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, m_CommandPool, callbacks);
        if (m_Sampler != VK_NULL_HANDLE)
            vkDestroySampler(device, m_Sampler, callbacks);
        if (m_Pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device, m_Pipeline, callbacks);
        if (m_PipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device, m_PipelineLayout, callbacks);
        if (m_TextureLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, m_TextureLayout, callbacks);
        if (m_PrimitiveLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, m_PrimitiveLayout, callbacks);
        if (m_OwnsDescriptorPool && m_DescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device, m_DescriptorPool, callbacks);
        m_Allocator.reset();
    }

    bool VulkanRenderer::CreateObjects(VkFormat colorFormat, VkFormat depthFormat, VkFormat stencilFormat)
    {
        const VkDevice device = m_Info.Device;
        const VkAllocationCallbacks* callbacks = m_Info.Allocator;

        // Set 0: the frame's primitive array. Set 1: the texture of a draw command.
        VkDescriptorSetLayoutBinding primitiveBinding = {};
        primitiveBinding.binding = 0;
        primitiveBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        primitiveBinding.descriptorCount = 1;
        primitiveBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &primitiveBinding;
        if (!Check(vkCreateDescriptorSetLayout(device, &layoutInfo, callbacks, &m_PrimitiveLayout),
                   "vkCreateDescriptorSetLayout"))
            return false;

        VkDescriptorSetLayoutBinding textureBinding = primitiveBinding;
        textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutInfo.pBindings = &textureBinding;
        if (!Check(vkCreateDescriptorSetLayout(device, &layoutInfo, callbacks, &m_TextureLayout),
                   "vkCreateDescriptorSetLayout"))
            return false;

        if (!CreatePipeline(colorFormat, depthFormat, stencilFormat))
            return false;

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 0.25f;
        if (!Check(vkCreateSampler(device, &samplerInfo, callbacks, &m_Sampler), "vkCreateSampler"))
            return false;

        m_DescriptorPool = m_Info.DescriptorPool;
        if (m_DescriptorPool == VK_NULL_HANDLE)
        {
            const std::array<VkDescriptorPoolSize, 2> sizes = {
                VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_Info.FramesInFlight},
                VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, PoolTextureSets}};
            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            poolInfo.maxSets = m_Info.FramesInFlight + PoolTextureSets;
            poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
            poolInfo.pPoolSizes = sizes.data();
            if (!Check(vkCreateDescriptorPool(device, &poolInfo, callbacks, &m_DescriptorPool),
                       "vkCreateDescriptorPool"))
                return false;
            m_OwnsDescriptorPool = true;
        }

        VkCommandPoolCreateInfo commandPoolInfo = {};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = m_Info.QueueFamily;
        if (!Check(vkCreateCommandPool(device, &commandPoolInfo, callbacks, &m_CommandPool), "vkCreateCommandPool"))
            return false;

        m_Slots.resize(m_Info.FramesInFlight);
        for (FrameSlot& slot : m_Slots)
        {
            VkDescriptorSetAllocateInfo allocateInfo = {};
            allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocateInfo.descriptorPool = m_DescriptorPool;
            allocateInfo.descriptorSetCount = 1;
            allocateInfo.pSetLayouts = &m_PrimitiveLayout;
            if (!Check(vkAllocateDescriptorSets(device, &allocateInfo, &slot.PrimitiveSet), "vkAllocateDescriptorSets"))
                return false;
        }

        // One more upload than frames in flight: an upload is rarely still running when its turn comes again.
        m_Uploads.resize(m_Info.FramesInFlight + 1);
        for (Upload& upload : m_Uploads)
        {
            VkCommandBufferAllocateInfo allocateInfo = {};
            allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocateInfo.commandPool = m_CommandPool;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateInfo.commandBufferCount = 1;
            if (!Check(vkAllocateCommandBuffers(device, &allocateInfo, &upload.CommandBuffer),
                       "vkAllocateCommandBuffers"))
                return false;
            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            if (!Check(vkCreateFence(device, &fenceInfo, callbacks, &upload.Fence), "vkCreateFence"))
                return false;
        }
        return true;
    }

    bool VulkanRenderer::CreatePipeline(VkFormat colorFormat, VkFormat depthFormat, VkFormat stencilFormat)
    {
        const VkDevice device = m_Info.Device;
        const VkAllocationCallbacks* callbacks = m_Info.Allocator;

        VkPushConstantRange pushConstants = {};
        pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstants.size = sizeof(FrameConstants);
        const std::array<VkDescriptorSetLayout, 2> setLayouts = {m_PrimitiveLayout, m_TextureLayout};
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        layoutInfo.pSetLayouts = setLayouts.data();
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstants;
        if (!Check(vkCreatePipelineLayout(device, &layoutInfo, callbacks, &m_PipelineLayout), "vkCreatePipelineLayout"))
            return false;

        const VkShaderModule vertexShader =
            CreateShaderModule(device, VertexShaderCode, sizeof(VertexShaderCode), callbacks);
        const VkShaderModule fragmentShader =
            CreateShaderModule(device, FragmentShaderCode, sizeof(FragmentShaderCode), callbacks);
        if (vertexShader == VK_NULL_HANDLE || fragmentShader == VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device, vertexShader, callbacks);
            vkDestroyShaderModule(device, fragmentShader, callbacks);
            return false;
        }

        std::array<VkPipelineShaderStageCreateInfo, 2> stages = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexShader;
        stages[0].pName = "main";
        stages[1] = stages[0];
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragmentShader;

        // Vertex layout: mirrors DrawVertex.
        static_assert(sizeof(DrawVertex) == 32, "DrawVertex must match the vertex layout below");
        static_assert(sizeof(DrawPrimitive) == 32, "DrawPrimitive must match `struct Primitive` in Carbon.frag");
        VkVertexInputBindingDescription binding = {};
        binding.stride = sizeof(DrawVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        const std::array<VkVertexInputAttributeDescription, 5> attributes = {
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(DrawVertex, Position)},
            VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(DrawVertex, Local)},
            VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(DrawVertex, UV)},
            VkVertexInputAttributeDescription{3, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(DrawVertex, Color)},
            VkVertexInputAttributeDescription{4, 0, VK_FORMAT_R32_UINT, offsetof(DrawVertex, Primitive)}};
        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization = {};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = static_cast<VkSampleCountFlagBits>(m_Info.SampleCount);

        // Carbon neither tests nor writes depth; the state is there for passes that have a depth attachment.
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

        // Premultiplied alpha blending.
        VkPipelineColorBlendAttachmentState blendAttachment = {};
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend = {};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;

        const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic = {};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();

        // Dynamic rendering: the attachment formats take the place of a render pass.
        VkPipelineRenderingCreateInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachmentFormats = &colorFormat;
        renderingInfo.depthAttachmentFormat = depthFormat;
        renderingInfo.stencilAttachmentFormat = stencilFormat;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = m_Info.RenderPass == VK_NULL_HANDLE ? &renderingInfo : nullptr;
        pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = m_Info.RenderPass;
        pipelineInfo.subpass = m_Info.Subpass;
        const bool isCreated =
            Check(vkCreateGraphicsPipelines(device, m_Info.PipelineCache, 1, &pipelineInfo, callbacks, &m_Pipeline),
                  "vkCreateGraphicsPipelines");

        vkDestroyShaderModule(device, vertexShader, callbacks);
        vkDestroyShaderModule(device, fragmentShader, callbacks);
        return isCreated;
    }

    bool VulkanRenderer::CreateBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (!Check(vkCreateBuffer(m_Info.Device, &bufferInfo, m_Info.Allocator, &buffer.Handle), "vkCreateBuffer"))
            return false;

        VkMemoryRequirements requirements = {};
        vkGetBufferMemoryRequirements(m_Info.Device, buffer.Handle, &requirements);
        if (!m_Allocator->Allocate(requirements, HostMemory, false, buffer.Memory) ||
            !Check(vkBindBufferMemory(m_Info.Device, buffer.Handle, buffer.Memory.Memory, buffer.Memory.Offset),
                   "vkBindBufferMemory"))
        {
            DestroyBuffer(buffer);
            return false;
        }
        buffer.Capacity = size;
        return true;
    }

    bool VulkanRenderer::EnsureBuffer(Buffer& buffer, VkDeviceSize requiredSize, VkBufferUsageFlags usage)
    {
        if (buffer.Handle != VK_NULL_HANDLE && buffer.Capacity >= requiredSize)
            return true;

        // Grow geometrically so steady-state frames never reallocate.
        VkDeviceSize capacity = std::max(buffer.Capacity * 2, MinimumBufferSize);
        while (capacity < requiredSize)
            capacity *= 2;
        RetireBuffer(buffer);
        return CreateBuffer(buffer, capacity, usage);
    }

    void VulkanRenderer::DestroyBuffer(Buffer& buffer)
    {
        if (buffer.Handle != VK_NULL_HANDLE)
            vkDestroyBuffer(m_Info.Device, buffer.Handle, m_Info.Allocator);
        m_Allocator->Free(buffer.Memory);
        buffer = Buffer();
    }

    void VulkanRenderer::RetireBuffer(Buffer& buffer)
    {
        if (buffer.Handle == VK_NULL_HANDLE)
            return;
        Retired retired;
        retired.Buffer = buffer.Handle;
        retired.Memory = buffer.Memory;
        Retire(retired);
        buffer = Buffer();
    }

    void VulkanRenderer::Retire(Retired retired)
    {
        retired.FrameNumber = m_FrameNumber;
        m_Retired.push_back(retired);
    }

    void VulkanRenderer::DestroyRetired(bool all)
    {
        // An object last used in frame N is free once frame N + FramesInFlight is being recorded: by then the host
        // has waited for frame N.
        for (auto it = m_Retired.begin(); it != m_Retired.end();)
        {
            if (!all && it->FrameNumber + m_Info.FramesInFlight > m_FrameNumber)
            {
                ++it;
                continue;
            }
            if (it->Set != VK_NULL_HANDLE)
                vkFreeDescriptorSets(m_Info.Device, m_DescriptorPool, 1, &it->Set);
            if (it->View != VK_NULL_HANDLE)
                vkDestroyImageView(m_Info.Device, it->View, m_Info.Allocator);
            if (it->Image != VK_NULL_HANDLE)
                vkDestroyImage(m_Info.Device, it->Image, m_Info.Allocator);
            if (it->Buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(m_Info.Device, it->Buffer, m_Info.Allocator);
            m_Allocator->Free(it->Memory);
            it = m_Retired.erase(it);
        }
    }

    VkDescriptorSet VulkanRenderer::AllocateTextureSet(VkImageView view, VkImageLayout layout)
    {
        VkDescriptorSetAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_DescriptorPool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &m_TextureLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (!Check(vkAllocateDescriptorSets(m_Info.Device, &allocateInfo, &set), "vkAllocateDescriptorSets"))
            return VK_NULL_HANDLE;

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.sampler = m_Sampler;
        imageInfo.imageView = view;
        imageInfo.imageLayout = layout;
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_Info.Device, 1, &write, 0, nullptr);
        return set;
    }

    bool VulkanRenderer::CreateAtlasImage(uint32_t width, uint32_t height)
    {
        // The old atlas may still be sampled by frames in flight.
        if (m_AtlasImage != VK_NULL_HANDLE)
        {
            Retired retired;
            retired.Image = m_AtlasImage;
            retired.View = m_AtlasView;
            retired.Set = m_AtlasSet;
            retired.Memory = m_AtlasMemory;
            Retire(retired);
        }
        m_AtlasImage = VK_NULL_HANDLE;
        m_AtlasView = VK_NULL_HANDLE;
        m_AtlasSet = VK_NULL_HANDLE;
        m_AtlasMemory = VulkanAllocation();
        m_AtlasWidth = 0;
        m_AtlasHeight = 0;
        m_IsAtlasInitialized = false;

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8_UNORM;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (!Check(vkCreateImage(m_Info.Device, &imageInfo, m_Info.Allocator, &m_AtlasImage), "vkCreateImage"))
            return false;

        VkMemoryRequirements requirements = {};
        vkGetImageMemoryRequirements(m_Info.Device, m_AtlasImage, &requirements);
        if (!m_Allocator->Allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, m_AtlasMemory) ||
            !Check(vkBindImageMemory(m_Info.Device, m_AtlasImage, m_AtlasMemory.Memory, m_AtlasMemory.Offset),
                   "vkBindImageMemory"))
            return false;

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_AtlasImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8_UNORM;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (!Check(vkCreateImageView(m_Info.Device, &viewInfo, m_Info.Allocator, &m_AtlasView), "vkCreateImageView"))
            return false;

        m_AtlasSet = AllocateTextureSet(m_AtlasView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (m_AtlasSet == VK_NULL_HANDLE)
            return false;
        m_AtlasWidth = width;
        m_AtlasHeight = height;
        return true;
    }

    RendererBackendCapabilities VulkanRenderer::GetCapabilities() const
    {
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(m_Info.PhysicalDevice, &properties);
        RendererBackendCapabilities capabilities;
        capabilities.MaxTextureSize = properties.limits.maxImageDimension2D;
        return capabilities;
    }

    void VulkanRenderer::EndFrame()
    {
        m_HasNewDrawData = true;
    }

    void VulkanRenderer::UpdateGlyphAtlas(const GlyphAtlasUpdate& update)
    {
        if (m_AtlasImage == VK_NULL_HANDLE || m_AtlasWidth != update.Width || m_AtlasHeight != update.Height)
        {
            if (!CreateAtlasImage(update.Width, update.Height))
            {
                InvalidateGlyphAtlas();
                return;
            }
        }

        // A new image has undefined contents, so it always gets every row.
        uint32_t firstRow = update.FirstRow;
        uint32_t rowCount = update.RowCount;
        if (!m_IsAtlasInitialized)
        {
            firstRow = 0;
            rowCount = update.Height;
        }
        if (rowCount == 0)
            return;

        Upload& upload = m_Uploads[m_NextUpload];
        m_NextUpload = (m_NextUpload + 1) % m_Uploads.size();
        if (upload.IsPending)
        {
            vkWaitForFences(m_Info.Device, 1, &upload.Fence, VK_TRUE, UINT64_MAX);
            vkResetFences(m_Info.Device, 1, &upload.Fence);
            upload.IsPending = false;
        }

        // The staging buffer of this upload is idle: it can be replaced at once.
        const VkDeviceSize size = static_cast<VkDeviceSize>(update.Width) * rowCount;
        if (upload.Staging.Capacity < size)
        {
            DestroyBuffer(upload.Staging);
            VkDeviceSize capacity = MinimumBufferSize;
            while (capacity < size)
                capacity *= 2;
            if (!CreateBuffer(upload.Staging, capacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
            {
                InvalidateGlyphAtlas();
                return;
            }
        }
        std::memcpy(upload.Staging.Memory.Mapped, update.Pixels.data() + static_cast<size_t>(firstRow) * update.Width,
                    static_cast<size_t>(size));

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(upload.CommandBuffer, &beginInfo);

        // Earlier frames may still sample the atlas: the copy waits for their fragment shaders.
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = m_IsAtlasInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_AtlasImage;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(upload.CommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageOffset = {0, static_cast<int32_t>(firstRow), 0};
        region.imageExtent = {update.Width, rowCount, 1};
        vkCmdCopyBufferToImage(upload.CommandBuffer, upload.Staging.Handle, m_AtlasImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Later frames, submitted after this, sample the new rows.
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(upload.CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        vkEndCommandBuffer(upload.CommandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &upload.CommandBuffer;
        if (!Check(vkQueueSubmit(m_Info.Queue, 1, &submitInfo, upload.Fence), "vkQueueSubmit"))
        {
            InvalidateGlyphAtlas();
            return;
        }
        upload.IsPending = true;
        m_IsAtlasInitialized = true;
    }

    void VulkanRenderer::Render(const DrawData& drawData)
    {
        CB_VERIFY(m_CommandBuffer != VK_NULL_HANDLE, "The Vulkan renderer has no command buffer to record into");
        if (m_CommandBuffer == VK_NULL_HANDLE || m_AtlasSet == VK_NULL_HANDLE)
            return;

        // A new frame takes the next slot; drawing the same frame again reuses what was uploaded.
        if (m_HasNewDrawData || !m_HasUploadedFrame)
        {
            m_FrameNumber++;
            DestroyRetired(false);
            FrameSlot& slot = m_Slots[m_FrameNumber % m_Slots.size()];

            const VkDeviceSize vertexBytes = drawData.Vertices.size_bytes();
            const VkDeviceSize indexBytes = drawData.Indices.size_bytes();
            const VkDeviceSize primitiveBytes =
                std::max<VkDeviceSize>(drawData.Primitives.size_bytes(), sizeof(DrawPrimitive));
            const VkBuffer oldPrimitives = slot.Primitives.Handle;
            if (!EnsureBuffer(slot.Vertices, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
                !EnsureBuffer(slot.Indices, indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT) ||
                !EnsureBuffer(slot.Primitives, primitiveBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
                return;
            if (slot.Primitives.Handle != oldPrimitives)
            {
                // The slot's set is idle: its last frame finished FramesInFlight frames ago.
                VkDescriptorBufferInfo bufferInfo = {slot.Primitives.Handle, 0, VK_WHOLE_SIZE};
                VkWriteDescriptorSet write = {};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = slot.PrimitiveSet;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.pBufferInfo = &bufferInfo;
                vkUpdateDescriptorSets(m_Info.Device, 1, &write, 0, nullptr);
            }

            std::memcpy(slot.Vertices.Memory.Mapped, drawData.Vertices.data(), static_cast<size_t>(vertexBytes));
            std::memcpy(slot.Indices.Memory.Mapped, drawData.Indices.data(), static_cast<size_t>(indexBytes));
            if (!drawData.Primitives.empty())
            {
                std::memcpy(slot.Primitives.Memory.Mapped, drawData.Primitives.data(),
                            drawData.Primitives.size_bytes());
            }
            m_HasNewDrawData = false;
            m_HasUploadedFrame = true;
        }
        const FrameSlot& slot = m_Slots[m_FrameNumber % m_Slots.size()];

        const float scale = drawData.ContentScale;
        const uint32_t targetWidth = static_cast<uint32_t>(std::lround(drawData.DisplaySize.X * scale));
        const uint32_t targetHeight = static_cast<uint32_t>(std::lround(drawData.DisplaySize.Y * scale));
        const VkCommandBuffer commands = m_CommandBuffer;

        vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
        const VkViewport viewport = {0.0f, 0.0f, static_cast<float>(targetWidth), static_cast<float>(targetHeight),
                                     0.0f, 1.0f};
        vkCmdSetViewport(commands, 0, 1, &viewport);
        const FrameConstants constants = {drawData.DisplaySize.X, drawData.DisplaySize.Y, scale,
                                          m_IsLinearOutput ? 1.0f : 0.0f};
        vkCmdPushConstants(commands, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(constants), &constants);
        vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &slot.PrimitiveSet,
                                0, nullptr);
        const VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(commands, 0, 1, &slot.Vertices.Handle, &vertexOffset);
        vkCmdBindIndexBuffer(commands, slot.Indices.Handle, 0, VK_INDEX_TYPE_UINT32);

        VkDescriptorSet boundSet = VK_NULL_HANDLE;
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

            VkDescriptorSet set = m_AtlasSet;
            if (command.Texture != TextureID())
            {
                // A texture not registered with VulkanGetTextureID is a raw VkImageView (MakeTextureID), sampled in
                // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
                HostTexture& texture = m_HostTextures[command.Texture.Value];
                if (texture.Set == VK_NULL_HANDLE)
                {
                    texture.View = ToView(command.Texture.Value);
                    texture.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    texture.Set = AllocateTextureSet(texture.View, texture.Layout);
                }
                if (texture.Set == VK_NULL_HANDLE)
                {
                    CB_LOG_WARNING("Renderer", "No descriptor set for a host texture; skipped");
                    continue;
                }
                set = texture.Set;
            }
            if (set != boundSet)
            {
                vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 1, 1, &set, 0,
                                        nullptr);
                boundSet = set;
            }

            const VkRect2D scissor = {{static_cast<int32_t>(left), static_cast<int32_t>(top)},
                                      {static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top)}};
            vkCmdSetScissor(commands, 0, 1, &scissor);
            vkCmdDrawIndexed(commands, command.IndexCount, 1, command.IndexOffset, 0, 0);
        }
    }

    TextureID VulkanRenderer::RegisterTexture(VkImageView view, VkImageLayout layout)
    {
        if (view == VK_NULL_HANDLE)
            return TextureID();
        const uint64_t key = GetTextureKey(view);
        HostTexture& texture = m_HostTextures[key];
        if (texture.Set == VK_NULL_HANDLE || texture.Layout != layout)
        {
            // A set that recorded frames may use is never changed: a new layout gets a new set.
            if (texture.Set != VK_NULL_HANDLE)
            {
                Retired retired;
                retired.Set = texture.Set;
                Retire(retired);
            }
            texture.View = view;
            texture.Layout = layout;
            texture.Set = AllocateTextureSet(view, layout);
            if (texture.Set == VK_NULL_HANDLE)
                CB_LOG_WARNING("Vulkan", "No descriptor set for a host texture; the descriptor pool may be full");
        }
        return RegisterHostTexture(key);
    }

    void VulkanRenderer::ReleaseTexture(TextureID texture)
    {
        const auto found = m_HostTextures.find(texture.Value);
        if (found == m_HostTextures.end())
            return;
        if (found->second.Set != VK_NULL_HANDLE)
        {
            Retired retired;
            retired.Set = found->second.Set;
            Retire(retired);
        }
        m_HostTextures.erase(found);
    }
} // namespace Carbon::Internal
