// The Vulkan device of the examples: instance, device, swapchain with frames in flight, or an offscreen image.
// VulkanMinimal shows the same setup with the host's own pipeline next to Carbon's.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Carbon/Backends/Vulkan/VulkanBackend.h>

#include "GraphicsDevice.h"

namespace Example
{
    namespace
    {
        // How many frames the CPU may record ahead of the GPU. Carbon is told the same number.
        constexpr uint32_t FramesInFlight = 2;

        bool Check(VkResult result, const char* what)
        {
            if (result == VK_SUCCESS)
                return true;
            std::fprintf(stderr, "%s failed (%d)\n", what, static_cast<int>(result));
            return false;
        }

        Carbon::TextureFormat ToCarbonFormat(VkFormat format)
        {
            switch (format)
            {
                case VK_FORMAT_R8G8B8A8_UNORM:
                    return Carbon::TextureFormat::RGBA8Unorm;
                case VK_FORMAT_R8G8B8A8_SRGB:
                    return Carbon::TextureFormat::RGBA8UnormSrgb;
                case VK_FORMAT_B8G8R8A8_SRGB:
                    return Carbon::TextureFormat::BGRA8UnormSrgb;
                default:
                    return Carbon::TextureFormat::BGRA8Unorm;
            }
        }

        class VulkanDevice : public GraphicsDevice
        {
        public:
            ~VulkanDevice() override
            {
                if (m_Device != VK_NULL_HANDLE)
                {
                    vkDeviceWaitIdle(m_Device);
                    DestroyTargets();
                    for (Frame& frame : m_Frames)
                    {
                        vkDestroyFence(m_Device, frame.Fence, nullptr);
                        vkDestroySemaphore(m_Device, frame.ImageAvailable, nullptr);
                    }
                    for (const Texture& texture : m_Textures)
                    {
                        vkDestroyImageView(m_Device, texture.View, nullptr);
                        vkDestroyImage(m_Device, texture.Image, nullptr);
                        vkFreeMemory(m_Device, texture.Memory, nullptr);
                    }
                    vkDestroyBuffer(m_Device, m_Readback, nullptr);
                    vkFreeMemory(m_Device, m_ReadbackMemory, nullptr);
                    vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
                    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
                    vkDestroyDevice(m_Device, nullptr);
                }
                if (m_Surface != VK_NULL_HANDLE)
                    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
                if (m_Instance != VK_NULL_HANDLE)
                    vkDestroyInstance(m_Instance, nullptr);
            }

            std::string_view GetName() const override { return "Vulkan"; }

            void SetWindowHints() const override { glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); }

            bool Create(GLFWwindow* window, bool isOffscreen, uint32_t width, uint32_t height) override
            {
                m_Window = isOffscreen ? nullptr : window;
                m_Width = width;
                m_Height = height;
                m_RequestedWidth = width;
                m_RequestedHeight = height;
                if (m_Window != nullptr && glfwVulkanSupported() == GLFW_FALSE)
                {
                    std::fprintf(stderr, "GLFW finds no Vulkan loader\n");
                    return false;
                }
                return CreateInstance() && CreateDevice() && CreateFrames() &&
                       (m_Window != nullptr ? CreateSwapchain() : CreateOffscreenTarget());
            }

            bool InitCarbon() override
            {
                Carbon::VulkanInitInfo info;
                info.Instance = m_Instance;
                info.PhysicalDevice = m_PhysicalDevice;
                info.Device = m_Device;
                info.Queue = m_Queue;
                info.QueueFamily = m_QueueFamily;
                info.FramesInFlight = FramesInFlight;
                info.RenderPass = m_RenderPass;
                info.ColorFormat = ToCarbonFormat(m_ColorFormat);
                return Carbon::VulkanInit(info);
            }

            void ShutdownCarbon() override
            {
                // The GPU must be done with Carbon's frames.
                vkDeviceWaitIdle(m_Device);
                Carbon::VulkanShutdown();
            }

            bool BeginFrame(uint32_t width, uint32_t height) override
            {
                // The window's framebuffer changed size: a new swapchain, whose extent the surface decides.
                if (m_Window != nullptr &&
                    (width != m_RequestedWidth || height != m_RequestedHeight || m_Swapchain == VK_NULL_HANDLE))
                {
                    m_RequestedWidth = width;
                    m_RequestedHeight = height;
                    RecreateSwapchain();
                }
                if (m_Window != nullptr && m_Swapchain == VK_NULL_HANDLE)
                    return false;

                Frame& frame = m_Frames[m_FrameIndex];
                // The GPU is done with the frame that used this slot FramesInFlight frames ago. Carbon relies on this.
                vkWaitForFences(m_Device, 1, &frame.Fence, VK_TRUE, UINT64_MAX);
                m_ImageIndex = 0;
                if (m_Window != nullptr)
                {
                    VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, frame.ImageAvailable,
                                                            VK_NULL_HANDLE, &m_ImageIndex);
                    if (result == VK_ERROR_OUT_OF_DATE_KHR)
                    {
                        RecreateSwapchain();
                        if (m_Swapchain == VK_NULL_HANDLE)
                            return false;
                        result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, frame.ImageAvailable,
                                                       VK_NULL_HANDLE, &m_ImageIndex);
                    }
                    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
                        return false;
                }
                vkResetFences(m_Device, 1, &frame.Fence);
                vkResetCommandBuffer(frame.Commands, 0);
                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(frame.Commands, &beginInfo);
                return true;
            }

            void Render(Carbon::Color background) override
            {
                const VkCommandBuffer commands = m_Frames[m_FrameIndex].Commands;
                VkClearValue clearValue = {};
                clearValue.color = {{background.R, background.G, background.B, 1.0f}};
                VkRenderPassBeginInfo passInfo = {};
                passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                passInfo.renderPass = m_RenderPass;
                passInfo.framebuffer = m_Targets[m_ImageIndex].Framebuffer;
                passInfo.renderArea = {{0, 0}, {m_Width, m_Height}};
                passInfo.clearValueCount = 1;
                passInfo.pClearValues = &clearValue;
                vkCmdBeginRenderPass(commands, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
                Carbon::VulkanRender(commands);
                vkCmdEndRenderPass(commands);
            }

            void EndFrame() override
            {
                Frame& frame = m_Frames[m_FrameIndex];
                if (m_Window == nullptr)
                {
                    // Offscreen, every frame is copied out; ReadPixels reads the last one.
                    VkBufferImageCopy region = {};
                    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                    region.imageExtent = {m_Width, m_Height, 1};
                    vkCmdCopyImageToBuffer(frame.Commands, m_Targets[0].Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                           m_Readback, 1, &region);
                }
                vkEndCommandBuffer(frame.Commands);

                const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo submitInfo = {};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &frame.Commands;
                if (m_Window != nullptr)
                {
                    submitInfo.waitSemaphoreCount = 1;
                    submitInfo.pWaitSemaphores = &frame.ImageAvailable;
                    submitInfo.pWaitDstStageMask = &waitStage;
                    submitInfo.signalSemaphoreCount = 1;
                    submitInfo.pSignalSemaphores = &m_Targets[m_ImageIndex].RenderFinished;
                }
                Check(vkQueueSubmit(m_Queue, 1, &submitInfo, frame.Fence), "vkQueueSubmit");

                if (m_Window != nullptr)
                {
                    VkPresentInfoKHR presentInfo = {};
                    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                    presentInfo.waitSemaphoreCount = 1;
                    presentInfo.pWaitSemaphores = &m_Targets[m_ImageIndex].RenderFinished;
                    presentInfo.swapchainCount = 1;
                    presentInfo.pSwapchains = &m_Swapchain;
                    presentInfo.pImageIndices = &m_ImageIndex;
                    const VkResult result = vkQueuePresentKHR(m_Queue, &presentInfo);
                    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
                        RecreateSwapchain();
                }
                m_FrameIndex = (m_FrameIndex + 1) % FramesInFlight;
            }

            bool ReadPixels(std::vector<uint8_t>& pixels) override
            {
                vkDeviceWaitIdle(m_Device);
                void* mapped = nullptr;
                if (!Check(vkMapMemory(m_Device, m_ReadbackMemory, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory"))
                    return false;
                pixels.resize(static_cast<size_t>(m_Width) * m_Height * 4);
                std::memcpy(pixels.data(), mapped, pixels.size());
                vkUnmapMemory(m_Device, m_ReadbackMemory);
                return true;
            }

            Carbon::TextureID CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
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
                if (!Check(vkCreateImage(m_Device, &imageInfo, nullptr, &texture.Image), "vkCreateImage"))
                    return {};
                VkMemoryRequirements requirements = {};
                vkGetImageMemoryRequirements(m_Device, texture.Image, &requirements);
                texture.Memory = Allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                vkBindImageMemory(m_Device, texture.Image, texture.Memory, 0);

                // The texels go through a staging buffer, and the image ends in the layout Carbon samples it in.
                VkBuffer staging = VK_NULL_HANDLE;
                VkBufferCreateInfo bufferInfo = {};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size = texels.size();
                bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                vkCreateBuffer(m_Device, &bufferInfo, nullptr, &staging);
                vkGetBufferMemoryRequirements(m_Device, staging, &requirements);
                const VkDeviceMemory stagingMemory =
                    Allocate(requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                vkBindBufferMemory(m_Device, staging, stagingMemory, 0);
                void* mapped = nullptr;
                vkMapMemory(m_Device, stagingMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
                std::memcpy(mapped, texels.data(), texels.size());
                vkUnmapMemory(m_Device, stagingMemory);

                VkCommandBufferAllocateInfo allocateInfo = {};
                allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocateInfo.commandPool = m_CommandPool;
                allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocateInfo.commandBufferCount = 1;
                VkCommandBuffer commands = VK_NULL_HANDLE;
                vkAllocateCommandBuffers(m_Device, &allocateInfo, &commands);
                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(commands, &beginInfo);
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = texture.Image;
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                     nullptr, 0, nullptr, 1, &barrier);
                VkBufferImageCopy region = {};
                region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                region.imageExtent = {width, height, 1};
                vkCmdCopyBufferToImage(commands, staging, texture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                       &region);
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                                     0, nullptr, 0, nullptr, 1, &barrier);
                vkEndCommandBuffer(commands);
                VkSubmitInfo submitInfo = {};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commands;
                vkQueueSubmit(m_Queue, 1, &submitInfo, VK_NULL_HANDLE);
                vkQueueWaitIdle(m_Queue);
                vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commands);
                vkDestroyBuffer(m_Device, staging, nullptr);
                vkFreeMemory(m_Device, stagingMemory, nullptr);

                VkImageViewCreateInfo viewInfo = {};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = texture.Image;
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                vkCreateImageView(m_Device, &viewInfo, nullptr, &texture.View);
                m_Textures.push_back(texture);
                // A raw view is sampled in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, where it is now.
                return Carbon::MakeTextureID(texture.View);
            }

        private:
            struct Frame
            {
                VkCommandBuffer Commands = VK_NULL_HANDLE;
                VkFence Fence = VK_NULL_HANDLE;
                VkSemaphore ImageAvailable = VK_NULL_HANDLE;
            };

            /// A swapchain image (or the offscreen image) with what rendering into it needs.
            struct Target
            {
                VkImage Image = VK_NULL_HANDLE;
                VkDeviceMemory Memory = VK_NULL_HANDLE; // offscreen only
                VkImageView View = VK_NULL_HANDLE;
                VkFramebuffer Framebuffer = VK_NULL_HANDLE;
                VkSemaphore RenderFinished = VK_NULL_HANDLE; // swapchain only
            };

            struct Texture
            {
                VkImage Image = VK_NULL_HANDLE;
                VkDeviceMemory Memory = VK_NULL_HANDLE;
                VkImageView View = VK_NULL_HANDLE;
            };

            bool CreateInstance()
            {
                std::vector<const char*> extensions;
                if (m_Window != nullptr)
                {
                    uint32_t count = 0;
                    const char** required = glfwGetRequiredInstanceExtensions(&count);
                    extensions.assign(required, required + count);
                }
                VkApplicationInfo application = {};
                application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                application.pApplicationName = "Carbon example";
                application.apiVersion = VK_API_VERSION_1_1;
                VkInstanceCreateInfo instanceInfo = {};
                instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                instanceInfo.pApplicationInfo = &application;
                instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
                instanceInfo.ppEnabledExtensionNames = extensions.data();
#if !defined(NDEBUG)
                // Debug builds use the validation layer when it is installed; it prints what it finds.
                const char* layer = "VK_LAYER_KHRONOS_validation";
                uint32_t layerCount = 0;
                vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
                std::vector<VkLayerProperties> layers(layerCount);
                vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
                for (const VkLayerProperties& properties : layers)
                {
                    if (std::strcmp(properties.layerName, layer) == 0)
                    {
                        instanceInfo.enabledLayerCount = 1;
                        instanceInfo.ppEnabledLayerNames = &layer;
                    }
                }
#endif
                if (!Check(vkCreateInstance(&instanceInfo, nullptr, &m_Instance), "vkCreateInstance"))
                    return false;
                return m_Window == nullptr || Check(glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface),
                                                    "glfwCreateWindowSurface");
            }

            bool CreateDevice()
            {
                uint32_t count = 0;
                vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
                std::vector<VkPhysicalDevice> devices(count);
                vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());
                for (VkPhysicalDevice device : devices)
                {
                    uint32_t familyCount = 0;
                    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
                    std::vector<VkQueueFamilyProperties> families(familyCount);
                    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());
                    for (uint32_t i = 0; i < familyCount && m_PhysicalDevice == VK_NULL_HANDLE; i++)
                    {
                        VkBool32 canPresent = VK_TRUE;
                        if (m_Surface != VK_NULL_HANDLE)
                            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &canPresent);
                        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && canPresent == VK_TRUE)
                        {
                            m_PhysicalDevice = device;
                            m_QueueFamily = i;
                        }
                    }
                }
                if (m_PhysicalDevice == VK_NULL_HANDLE)
                {
                    std::fprintf(stderr, "No Vulkan device can render%s\n",
                                 m_Window != nullptr ? " to the window" : "");
                    return false;
                }

                const float priority = 1.0f;
                VkDeviceQueueCreateInfo queueInfo = {};
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.queueFamilyIndex = m_QueueFamily;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &priority;
                const char* swapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
                VkDeviceCreateInfo deviceInfo = {};
                deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                deviceInfo.queueCreateInfoCount = 1;
                deviceInfo.pQueueCreateInfos = &queueInfo;
                if (m_Window != nullptr)
                {
                    deviceInfo.enabledExtensionCount = 1;
                    deviceInfo.ppEnabledExtensionNames = &swapchainExtension;
                }
                if (!Check(vkCreateDevice(m_PhysicalDevice, &deviceInfo, nullptr, &m_Device), "vkCreateDevice"))
                    return false;
                vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);
                return true;
            }

            bool CreateFrames()
            {
                VkCommandPoolCreateInfo poolInfo = {};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = m_QueueFamily;
                if (!Check(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool), "vkCreateCommandPool"))
                    return false;
                for (Frame& frame : m_Frames)
                {
                    VkCommandBufferAllocateInfo allocateInfo = {};
                    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                    allocateInfo.commandPool = m_CommandPool;
                    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                    allocateInfo.commandBufferCount = 1;
                    vkAllocateCommandBuffers(m_Device, &allocateInfo, &frame.Commands);
                    VkFenceCreateInfo fenceInfo = {};
                    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                    vkCreateFence(m_Device, &fenceInfo, nullptr, &frame.Fence);
                    VkSemaphoreCreateInfo semaphoreInfo = {};
                    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                    vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &frame.ImageAvailable);
                }
                return true;
            }

            bool CreateRenderPass(VkImageLayout finalLayout)
            {
                if (m_RenderPass != VK_NULL_HANDLE)
                    return true; // the format never changes, so the render pass outlives swapchain recreation
                VkAttachmentDescription attachment = {};
                attachment.format = m_ColorFormat;
                attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.finalLayout = finalLayout;
                VkAttachmentReference reference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
                VkSubpassDescription subpass = {};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &reference;
                // Writing waits for the swapchain image; reading back waits for the writes.
                VkSubpassDependency dependencies[2] = {};
                dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
                dependencies[0].dstSubpass = 0;
                dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependencies[1].srcSubpass = 0;
                dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
                dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                VkRenderPassCreateInfo renderPassInfo = {};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                renderPassInfo.attachmentCount = 1;
                renderPassInfo.pAttachments = &attachment;
                renderPassInfo.subpassCount = 1;
                renderPassInfo.pSubpasses = &subpass;
                renderPassInfo.dependencyCount = 2;
                renderPassInfo.pDependencies = dependencies;
                return Check(vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass),
                             "vkCreateRenderPass");
            }

            bool CreateFramebuffer(Target& target)
            {
                VkImageViewCreateInfo viewInfo = {};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = target.Image;
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = m_ColorFormat;
                viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                if (!Check(vkCreateImageView(m_Device, &viewInfo, nullptr, &target.View), "vkCreateImageView"))
                    return false;
                VkFramebufferCreateInfo framebufferInfo = {};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = m_RenderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = &target.View;
                framebufferInfo.width = m_Width;
                framebufferInfo.height = m_Height;
                framebufferInfo.layers = 1;
                return Check(vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &target.Framebuffer),
                             "vkCreateFramebuffer");
            }

            bool CreateSwapchain()
            {
                // Prefer a non-sRGB format: Carbon then blends in gamma space, like macOS UI.
                uint32_t formatCount = 0;
                vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
                std::vector<VkSurfaceFormatKHR> formats(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());
                if (formats.empty())
                    return false;
                VkSurfaceFormatKHR surfaceFormat = formats[0];
                for (const VkSurfaceFormatKHR& format : formats)
                {
                    if (format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM)
                    {
                        surfaceFormat = format;
                        break;
                    }
                }
                if (m_RenderPass == VK_NULL_HANDLE)
                    m_ColorFormat = surfaceFormat.format;

                VkSurfaceCapabilitiesKHR capabilities = {};
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);
                int framebufferWidth = 0;
                int framebufferHeight = 0;
                glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);
                m_Width = capabilities.currentExtent.width != UINT32_MAX ? capabilities.currentExtent.width
                                                                         : static_cast<uint32_t>(framebufferWidth);
                m_Height = capabilities.currentExtent.height != UINT32_MAX ? capabilities.currentExtent.height
                                                                           : static_cast<uint32_t>(framebufferHeight);
                if (m_Width == 0 || m_Height == 0)
                    return true; // minimized; BeginFrame tries again

                VkSwapchainCreateInfoKHR swapchainInfo = {};
                swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
                swapchainInfo.surface = m_Surface;
                swapchainInfo.minImageCount = capabilities.minImageCount + 1;
                if (capabilities.maxImageCount > 0)
                    swapchainInfo.minImageCount = std::min(swapchainInfo.minImageCount, capabilities.maxImageCount);
                swapchainInfo.imageFormat = surfaceFormat.format;
                swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
                swapchainInfo.imageExtent = {m_Width, m_Height};
                swapchainInfo.imageArrayLayers = 1;
                swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                swapchainInfo.preTransform = capabilities.currentTransform;
                swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
                swapchainInfo.clipped = VK_TRUE;
                if (!Check(vkCreateSwapchainKHR(m_Device, &swapchainInfo, nullptr, &m_Swapchain),
                           "vkCreateSwapchainKHR") ||
                    !CreateRenderPass(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR))
                    return false;

                uint32_t imageCount = 0;
                vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
                std::vector<VkImage> images(imageCount);
                vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, images.data());
                m_Targets.resize(imageCount);
                for (uint32_t i = 0; i < imageCount; i++)
                {
                    m_Targets[i].Image = images[i];
                    VkSemaphoreCreateInfo semaphoreInfo = {};
                    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                    vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_Targets[i].RenderFinished);
                    if (!CreateFramebuffer(m_Targets[i]))
                        return false;
                }
                return true;
            }

            bool CreateOffscreenTarget()
            {
                // RGBA so the pixels can be written to a PNG without conversion.
                m_ColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
                if (!CreateRenderPass(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
                    return false;
                Target target;
                VkImageCreateInfo imageInfo = {};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.format = m_ColorFormat;
                imageInfo.extent = {m_Width, m_Height, 1};
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = 1;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                if (!Check(vkCreateImage(m_Device, &imageInfo, nullptr, &target.Image), "vkCreateImage"))
                    return false;
                VkMemoryRequirements requirements = {};
                vkGetImageMemoryRequirements(m_Device, target.Image, &requirements);
                target.Memory = Allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                vkBindImageMemory(m_Device, target.Image, target.Memory, 0);
                if (!CreateFramebuffer(target))
                    return false;
                m_Targets.push_back(target);

                VkBufferCreateInfo bufferInfo = {};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size = static_cast<VkDeviceSize>(m_Width) * m_Height * 4;
                bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                vkCreateBuffer(m_Device, &bufferInfo, nullptr, &m_Readback);
                vkGetBufferMemoryRequirements(m_Device, m_Readback, &requirements);
                m_ReadbackMemory =
                    Allocate(requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                vkBindBufferMemory(m_Device, m_Readback, m_ReadbackMemory, 0);
                return true;
            }

            VkDeviceMemory Allocate(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties) const
            {
                VkPhysicalDeviceMemoryProperties memory = {};
                vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memory);
                uint32_t type = 0;
                for (uint32_t i = 0; i < memory.memoryTypeCount; i++)
                {
                    if ((requirements.memoryTypeBits & (1u << i)) != 0 &&
                        (memory.memoryTypes[i].propertyFlags & properties) == properties)
                    {
                        type = i;
                        break;
                    }
                }
                VkMemoryAllocateInfo allocateInfo = {};
                allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocateInfo.allocationSize = requirements.size;
                allocateInfo.memoryTypeIndex = type;
                VkDeviceMemory result = VK_NULL_HANDLE;
                vkAllocateMemory(m_Device, &allocateInfo, nullptr, &result);
                return result;
            }

            void RecreateSwapchain()
            {
                vkDeviceWaitIdle(m_Device);
                DestroyTargets();
                CreateSwapchain();
            }

            void DestroyTargets()
            {
                for (Target& target : m_Targets)
                {
                    vkDestroyFramebuffer(m_Device, target.Framebuffer, nullptr);
                    vkDestroyImageView(m_Device, target.View, nullptr);
                    vkDestroySemaphore(m_Device, target.RenderFinished, nullptr);
                    if (target.Memory != VK_NULL_HANDLE)
                    {
                        vkDestroyImage(m_Device, target.Image, nullptr);
                        vkFreeMemory(m_Device, target.Memory, nullptr);
                    }
                }
                m_Targets.clear();
                if (m_Swapchain != VK_NULL_HANDLE)
                    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
                m_Swapchain = VK_NULL_HANDLE;
            }

        private:
            GLFWwindow* m_Window = nullptr;
            VkInstance m_Instance = VK_NULL_HANDLE;
            VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
            VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
            VkDevice m_Device = VK_NULL_HANDLE;
            VkQueue m_Queue = VK_NULL_HANDLE;
            uint32_t m_QueueFamily = 0;
            VkCommandPool m_CommandPool = VK_NULL_HANDLE;
            Frame m_Frames[FramesInFlight];
            uint32_t m_FrameIndex = 0;
            VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
            std::vector<Target> m_Targets;
            uint32_t m_ImageIndex = 0;
            VkFormat m_ColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
            VkRenderPass m_RenderPass = VK_NULL_HANDLE;
            VkBuffer m_Readback = VK_NULL_HANDLE;
            VkDeviceMemory m_ReadbackMemory = VK_NULL_HANDLE;
            std::vector<Texture> m_Textures;
            uint32_t m_Width = 0;
            uint32_t m_Height = 0;
            uint32_t m_RequestedWidth = 0;
            uint32_t m_RequestedHeight = 0;
        };
    } // namespace

    std::unique_ptr<GraphicsDevice> CreateGraphicsDevice()
    {
        return std::make_unique<VulkanDevice>();
    }
} // namespace Example
