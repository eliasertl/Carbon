#include "Support/BackendHarness.h"

#if defined(CARBON_HAS_BACKEND_VULKAN)

#include <algorithm>
#include <cstring>
#include <format>

#include <vulkan/vulkan.h>

#include "Carbon/Backends/Vulkan/VulkanBackend.h"

namespace Carbon
{
    namespace
    {
        constexpr const char* ValidationLayer = "VK_LAYER_KHRONOS_validation";

        VkFormat ToVulkan(TextureFormat format)
        {
            switch (format)
            {
                case TextureFormat::RGBA8UnormSrgb:
                    return VK_FORMAT_R8G8B8A8_SRGB;
                case TextureFormat::BGRA8Unorm:
                    return VK_FORMAT_B8G8R8A8_UNORM;
                case TextureFormat::BGRA8UnormSrgb:
                    return VK_FORMAT_B8G8R8A8_SRGB;
                default:
                    return VK_FORMAT_R8G8B8A8_UNORM;
            }
        }

        /// Drives the Vulkan backend on a device without a surface. With `useRenderPass` the backend's pipeline is
        /// built for a VkRenderPass, otherwise for dynamic rendering (which needs Vulkan 1.3). The Khronos
        /// validation layer is enabled when it is installed, and everything it reports is collected.
        class VulkanHarness : public BackendHarness
        {
        public:
            explicit VulkanHarness(bool useRenderPass) : m_UseRenderPass(useRenderPass) {}

            ~VulkanHarness() override
            {
                if (m_Device != VK_NULL_HANDLE)
                {
                    vkDeviceWaitIdle(m_Device);
                    for (Texture& texture : m_Textures)
                    {
                        vkDestroyImageView(m_Device, texture.View, nullptr);
                        vkDestroyImage(m_Device, texture.Image, nullptr);
                        vkFreeMemory(m_Device, texture.Memory, nullptr);
                    }
                    if (m_RenderPass != VK_NULL_HANDLE)
                        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
                    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
                    vkDestroyFence(m_Device, m_Fence, nullptr);
                    vkDestroyDevice(m_Device, nullptr);
                }
                if (m_Messenger != VK_NULL_HANDLE)
                {
                    const auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                        vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
                    destroyMessenger(m_Instance, m_Messenger, nullptr);
                }
                if (m_Instance != VK_NULL_HANDLE)
                    vkDestroyInstance(m_Instance, nullptr);
            }

            std::string_view GetName() const override { return m_UseRenderPass ? "VulkanRenderPass" : "Vulkan"; }

            std::string CreateDevice() override
            {
                uint32_t apiVersion = VK_API_VERSION_1_0;
                vkEnumerateInstanceVersion(&apiVersion);
                if (!m_UseRenderPass && apiVersion < VK_API_VERSION_1_3)
                    return "Dynamic rendering needs Vulkan 1.3";

                uint32_t layerCount = 0;
                vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
                std::vector<VkLayerProperties> layers(layerCount);
                vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
                const bool hasValidation = std::any_of(layers.begin(), layers.end(), [](const VkLayerProperties& layer)
                                                       { return std::strcmp(layer.layerName, ValidationLayer) == 0; });

                VkApplicationInfo application = {};
                application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                application.pApplicationName = "CarbonTests";
                application.apiVersion = apiVersion >= VK_API_VERSION_1_3 ? VK_API_VERSION_1_3 : VK_API_VERSION_1_0;
                const char* extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
                VkInstanceCreateInfo instanceInfo = {};
                instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                instanceInfo.pApplicationInfo = &application;
                if (hasValidation)
                {
                    instanceInfo.enabledLayerCount = 1;
                    instanceInfo.ppEnabledLayerNames = &ValidationLayer;
                    instanceInfo.enabledExtensionCount = 1;
                    instanceInfo.ppEnabledExtensionNames = &extension;
                }
                if (vkCreateInstance(&instanceInfo, nullptr, &m_Instance) != VK_SUCCESS)
                    return "No Vulkan instance could be created";

                if (hasValidation)
                {
                    VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
                    messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                    messengerInfo.messageSeverity =
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                    messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                    messengerInfo.pfnUserCallback =
                        [](VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                           const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData) -> VkBool32
                    {
                        static_cast<VulkanHarness*>(userData)->m_Messages.emplace_back(data->pMessage);
                        return VK_FALSE;
                    };
                    messengerInfo.pUserData = this;
                    const auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
                    if (createMessenger != nullptr)
                        createMessenger(m_Instance, &messengerInfo, nullptr, &m_Messenger);
                }

                uint32_t deviceCount = 0;
                vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
                std::vector<VkPhysicalDevice> devices(deviceCount);
                vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());
                for (VkPhysicalDevice device : devices)
                {
                    VkPhysicalDeviceProperties properties = {};
                    vkGetPhysicalDeviceProperties(device, &properties);
                    if (!m_UseRenderPass && properties.apiVersion < VK_API_VERSION_1_3)
                        continue;
                    uint32_t familyCount = 0;
                    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
                    std::vector<VkQueueFamilyProperties> families(familyCount);
                    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());
                    for (uint32_t i = 0; i < familyCount; i++)
                    {
                        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
                        {
                            m_PhysicalDevice = device;
                            m_QueueFamily = i;
                            break;
                        }
                    }
                    if (m_PhysicalDevice != VK_NULL_HANDLE)
                        break;
                }
                if (m_PhysicalDevice == VK_NULL_HANDLE)
                    return m_UseRenderPass ? "No Vulkan device with a graphics queue"
                                           : "No Vulkan 1.3 device with a graphics queue";

                const float priority = 1.0f;
                VkDeviceQueueCreateInfo queueInfo = {};
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.queueFamilyIndex = m_QueueFamily;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &priority;
                VkPhysicalDeviceVulkan13Features features13 = {};
                features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                features13.dynamicRendering = VK_TRUE;
                VkDeviceCreateInfo deviceInfo = {};
                deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                deviceInfo.pNext = m_UseRenderPass ? nullptr : &features13;
                deviceInfo.queueCreateInfoCount = 1;
                deviceInfo.pQueueCreateInfos = &queueInfo;
                if (vkCreateDevice(m_PhysicalDevice, &deviceInfo, nullptr, &m_Device) != VK_SUCCESS)
                    return "No Vulkan device could be created";
                vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);

                VkCommandPoolCreateInfo poolInfo = {};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = m_QueueFamily;
                vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool);
                VkCommandBufferAllocateInfo allocateInfo = {};
                allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocateInfo.commandPool = m_CommandPool;
                allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocateInfo.commandBufferCount = 1;
                vkAllocateCommandBuffers(m_Device, &allocateInfo, &m_CommandBuffer);
                VkFenceCreateInfo fenceInfo = {};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                vkCreateFence(m_Device, &fenceInfo, nullptr, &m_Fence);
                return {};
            }

            bool InitBackend(TextureFormat colorFormat) override
            {
                m_ColorFormat = ToVulkan(colorFormat);
                if (m_UseRenderPass)
                {
                    if (m_RenderPass != VK_NULL_HANDLE)
                        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
                    m_RenderPass = CreateRenderPass(m_ColorFormat);
                }

                VulkanInitInfo info;
                info.Instance = m_Instance;
                info.PhysicalDevice = m_PhysicalDevice;
                info.Device = m_Device;
                info.Queue = m_Queue;
                info.QueueFamily = m_QueueFamily;
                info.FramesInFlight = 2;
                info.RenderPass = m_RenderPass;
                info.ColorFormat = colorFormat;
                return VulkanInit(info);
            }

            void ShutdownBackend() override
            {
                vkDeviceWaitIdle(m_Device);
                VulkanShutdown();
            }

            RenderedImage RenderFrame(uint32_t width, uint32_t height, Color background) override
            {
                RenderedImage image;
                image.Width = width;
                image.Height = height;

                // The target, and a buffer to read it back into.
                VkImageCreateInfo imageInfo = {};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.format = m_ColorFormat;
                imageInfo.extent = {width, height, 1};
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = 1;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                VkImage target = VK_NULL_HANDLE;
                vkCreateImage(m_Device, &imageInfo, nullptr, &target);
                const VkDeviceMemory targetMemory = AllocateFor(target);
                const VkImageView targetView = CreateView(target, m_ColorFormat);

                const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(width) * height * 4;
                VkBuffer readback = VK_NULL_HANDLE;
                const VkDeviceMemory readbackMemory =
                    CreateHostBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, readback);

                VkFramebuffer framebuffer = VK_NULL_HANDLE;
                if (m_UseRenderPass)
                {
                    VkFramebufferCreateInfo framebufferInfo = {};
                    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                    framebufferInfo.renderPass = m_RenderPass;
                    framebufferInfo.attachmentCount = 1;
                    framebufferInfo.pAttachments = &targetView;
                    framebufferInfo.width = width;
                    framebufferInfo.height = height;
                    framebufferInfo.layers = 1;
                    vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &framebuffer);
                }

                const VkCommandBuffer commands = BeginCommands();
                VkClearValue clear = {};
                clear.color = {{background.R, background.G, background.B, background.A}};
                if (m_UseRenderPass)
                {
                    VkRenderPassBeginInfo beginInfo = {};
                    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    beginInfo.renderPass = m_RenderPass;
                    beginInfo.framebuffer = framebuffer;
                    beginInfo.renderArea = {{0, 0}, {width, height}};
                    beginInfo.clearValueCount = 1;
                    beginInfo.pClearValues = &clear;
                    vkCmdBeginRenderPass(commands, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
                    VulkanRender(commands);
                    vkCmdEndRenderPass(commands);
                }
                else
                {
                    Transition(commands, target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                    VkRenderingAttachmentInfo attachment = {};
                    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                    attachment.imageView = targetView;
                    attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                    attachment.clearValue = clear;
                    VkRenderingInfo renderingInfo = {};
                    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                    renderingInfo.renderArea = {{0, 0}, {width, height}};
                    renderingInfo.layerCount = 1;
                    renderingInfo.colorAttachmentCount = 1;
                    renderingInfo.pColorAttachments = &attachment;
                    vkCmdBeginRendering(commands, &renderingInfo);
                    VulkanRender(commands);
                    vkCmdEndRendering(commands);
                    Transition(commands, target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT);
                }

                VkBufferImageCopy region = {};
                region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                region.imageExtent = {width, height, 1};
                vkCmdCopyImageToBuffer(commands, target, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &region);
                SubmitAndWait(commands);

                void* mapped = nullptr;
                vkMapMemory(m_Device, readbackMemory, 0, bufferSize, 0, &mapped);
                image.Pixels.resize(static_cast<size_t>(bufferSize));
                std::memcpy(image.Pixels.data(), mapped, image.Pixels.size());
                vkUnmapMemory(m_Device, readbackMemory);
                if (m_ColorFormat == VK_FORMAT_B8G8R8A8_UNORM || m_ColorFormat == VK_FORMAT_B8G8R8A8_SRGB)
                {
                    for (size_t i = 0; i < image.Pixels.size(); i += 4)
                        std::swap(image.Pixels[i], image.Pixels[i + 2]);
                }

                if (framebuffer != VK_NULL_HANDLE)
                    vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
                vkDestroyBuffer(m_Device, readback, nullptr);
                vkFreeMemory(m_Device, readbackMemory, nullptr);
                vkDestroyImageView(m_Device, targetView, nullptr);
                vkDestroyImage(m_Device, target, nullptr);
                vkFreeMemory(m_Device, targetMemory, nullptr);
                return image;
            }

            size_t CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
            {
                Texture texture;
                VkImageCreateInfo imageInfo = {};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                imageInfo.extent = {width, height, 1};
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = 1;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                vkCreateImage(m_Device, &imageInfo, nullptr, &texture.Image);
                texture.Memory = AllocateFor(texture.Image);
                texture.View = CreateView(texture.Image, VK_FORMAT_R8G8B8A8_UNORM);

                VkBuffer staging = VK_NULL_HANDLE;
                const VkDeviceMemory stagingMemory =
                    CreateHostBuffer(texels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging);
                void* mapped = nullptr;
                vkMapMemory(m_Device, stagingMemory, 0, texels.size(), 0, &mapped);
                std::memcpy(mapped, texels.data(), texels.size());
                vkUnmapMemory(m_Device, stagingMemory);

                const VkCommandBuffer commands = BeginCommands();
                Transition(commands, texture.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
                VkBufferImageCopy region = {};
                region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                region.imageExtent = {width, height, 1};
                vkCmdCopyBufferToImage(commands, staging, texture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                       &region);
                Transition(commands, texture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                SubmitAndWait(commands);
                vkDestroyBuffer(m_Device, staging, nullptr);
                vkFreeMemory(m_Device, stagingMemory, nullptr);

                m_Textures.push_back(texture);
                return m_Textures.size() - 1;
            }

            TextureID GetTextureID(size_t texture) override { return VulkanGetTextureID(m_Textures[texture].View); }

            std::vector<std::string> TakeMessages() override
            {
                std::vector<std::string> messages = std::move(m_Messages);
                m_Messages.clear();
                return messages;
            }

        private:
            struct Texture
            {
                VkImage Image = VK_NULL_HANDLE;
                VkDeviceMemory Memory = VK_NULL_HANDLE;
                VkImageView View = VK_NULL_HANDLE;
            };

            VkRenderPass CreateRenderPass(VkFormat format) const
            {
                VkAttachmentDescription attachment = {};
                attachment.format = format;
                attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                VkAttachmentReference reference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
                VkSubpassDescription subpass = {};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &reference;
                // The copy after the pass reads what the pass wrote.
                VkSubpassDependency dependency = {};
                dependency.srcSubpass = 0;
                dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
                dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                VkRenderPassCreateInfo renderPassInfo = {};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                renderPassInfo.attachmentCount = 1;
                renderPassInfo.pAttachments = &attachment;
                renderPassInfo.subpassCount = 1;
                renderPassInfo.pSubpasses = &subpass;
                renderPassInfo.dependencyCount = 1;
                renderPassInfo.pDependencies = &dependency;
                VkRenderPass renderPass = VK_NULL_HANDLE;
                vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &renderPass);
                return renderPass;
            }

            uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const
            {
                VkPhysicalDeviceMemoryProperties memory = {};
                vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memory);
                for (uint32_t i = 0; i < memory.memoryTypeCount; i++)
                {
                    if ((typeBits & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & properties) == properties)
                        return i;
                }
                return 0;
            }

            VkDeviceMemory AllocateFor(VkImage image) const
            {
                VkMemoryRequirements requirements = {};
                vkGetImageMemoryRequirements(m_Device, image, &requirements);
                VkMemoryAllocateInfo allocateInfo = {};
                allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocateInfo.allocationSize = requirements.size;
                allocateInfo.memoryTypeIndex =
                    FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                VkDeviceMemory memory = VK_NULL_HANDLE;
                vkAllocateMemory(m_Device, &allocateInfo, nullptr, &memory);
                vkBindImageMemory(m_Device, image, memory, 0);
                return memory;
            }

            VkDeviceMemory CreateHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer) const
            {
                VkBufferCreateInfo bufferInfo = {};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size = size;
                bufferInfo.usage = usage;
                vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer);
                VkMemoryRequirements requirements = {};
                vkGetBufferMemoryRequirements(m_Device, buffer, &requirements);
                VkMemoryAllocateInfo allocateInfo = {};
                allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocateInfo.allocationSize = requirements.size;
                allocateInfo.memoryTypeIndex =
                    FindMemoryType(requirements.memoryTypeBits,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                VkDeviceMemory memory = VK_NULL_HANDLE;
                vkAllocateMemory(m_Device, &allocateInfo, nullptr, &memory);
                vkBindBufferMemory(m_Device, buffer, memory, 0);
                return memory;
            }

            VkImageView CreateView(VkImage image, VkFormat format) const
            {
                VkImageViewCreateInfo viewInfo = {};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = image;
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = format;
                viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                VkImageView view = VK_NULL_HANDLE;
                vkCreateImageView(m_Device, &viewInfo, nullptr, &view);
                return view;
            }

            static void Transition(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to,
                                   VkAccessFlags sourceAccess, VkAccessFlags destinationAccess,
                                   VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage)
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcAccessMask = sourceAccess;
                barrier.dstAccessMask = destinationAccess;
                barrier.oldLayout = from;
                barrier.newLayout = to;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                vkCmdPipelineBarrier(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            }

            VkCommandBuffer BeginCommands() const
            {
                vkResetCommandBuffer(m_CommandBuffer, 0);
                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
                return m_CommandBuffer;
            }

            void SubmitAndWait(VkCommandBuffer commands)
            {
                vkEndCommandBuffer(commands);
                VkSubmitInfo submitInfo = {};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commands;
                if (vkQueueSubmit(m_Queue, 1, &submitInfo, m_Fence) != VK_SUCCESS)
                {
                    m_Messages.emplace_back("vkQueueSubmit failed");
                    return;
                }
                vkWaitForFences(m_Device, 1, &m_Fence, VK_TRUE, UINT64_MAX);
                vkResetFences(m_Device, 1, &m_Fence);
            }

        private:
            bool m_UseRenderPass;
            VkInstance m_Instance = VK_NULL_HANDLE;
            VkDebugUtilsMessengerEXT m_Messenger = VK_NULL_HANDLE;
            VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
            VkDevice m_Device = VK_NULL_HANDLE;
            VkQueue m_Queue = VK_NULL_HANDLE;
            uint32_t m_QueueFamily = 0;
            VkCommandPool m_CommandPool = VK_NULL_HANDLE;
            VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
            VkFence m_Fence = VK_NULL_HANDLE;
            VkRenderPass m_RenderPass = VK_NULL_HANDLE;
            VkFormat m_ColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
            std::vector<Texture> m_Textures;
            std::vector<std::string> m_Messages;
        };
    } // namespace

    std::unique_ptr<BackendHarness> CreateVulkanHarness(bool useRenderPass)
    {
        return std::make_unique<VulkanHarness>(useRenderPass);
    }
} // namespace Carbon

#endif
