// Minimal integration of Carbon into a host application that renders with Vulkan.
//
// The host owns the window, the Vulkan device, the swapchain and the render pass. Each frame it forwards input to
// Carbon, builds the interface, draws its own content into the render pass and then lets Carbon add the interface
// on top, in the same render pass. The Vulkan chores (instance, device, swapchain, frames in flight) live in the
// VulkanHost class below; everything Carbon-specific is in main(). WebGPUMinimalIntegration and
// OpenGLMinimalIntegration do the same with the other backends.
//
//   VulkanMinimalIntegration [--theme light|dark] [--scale <factor>] [--size <w>x<h>] [--screenshot <file.png>]

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Carbon/Backends/Vulkan/VulkanBackend.h>
#include <Carbon/Carbon.h>

#include "ExampleArguments.h"
#include "GlfwInput.h"
#include "Screenshot.h"

namespace
{
    // How many frames the CPU may record ahead of the GPU. Carbon is told the same number.
    constexpr uint32_t FramesInFlight = 2;

    const uint32_t TriangleVertexCode[] = {
#include "Triangle.vert.inc"
    };
    const uint32_t TriangleFragmentCode[] = {
#include "Triangle.frag.inc"
    };

    bool Check(VkResult result, const char* what)
    {
        if (result == VK_SUCCESS)
            return true;
        std::fprintf(stderr, "%s failed (%d)\n", what, static_cast<int>(result));
        return false;
    }

    // ---------------------------------------------------------------------------------------------------------
    // The host's Vulkan setup. Nothing in here is specific to Carbon: an application already has all of it.
    // ---------------------------------------------------------------------------------------------------------
    class VulkanHost
    {
    public:
        /// With a window, renders into its swapchain; without one (screenshot mode), into an offscreen image of
        /// `width` x `height` pixels.
        bool Create(GLFWwindow* window, uint32_t width, uint32_t height)
        {
            m_Window = window;
            m_Width = width;
            m_Height = height;
            return CreateInstance() && CreateDevice() && CreateFrames() &&
                   (window != nullptr ? CreateSwapchain() : CreateOffscreenTarget()) && CreateTrianglePipeline();
        }

        ~VulkanHost()
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
                vkDestroyBuffer(m_Device, m_Readback, nullptr);
                vkFreeMemory(m_Device, m_ReadbackMemory, nullptr);
                vkDestroyPipeline(m_Device, m_TrianglePipeline, nullptr);
                vkDestroyPipelineLayout(m_Device, m_TriangleLayout, nullptr);
                vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
                vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
                vkDestroyDevice(m_Device, nullptr);
            }
            if (m_Surface != VK_NULL_HANDLE)
                vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            if (m_Instance != VK_NULL_HANDLE)
                vkDestroyInstance(m_Instance, nullptr);
        }

        VkInstance GetInstance() const { return m_Instance; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkDevice GetDevice() const { return m_Device; }
        VkQueue GetQueue() const { return m_Queue; }
        uint32_t GetQueueFamily() const { return m_QueueFamily; }
        VkRenderPass GetRenderPass() const { return m_RenderPass; }
        VkFormat GetColorFormat() const { return m_ColorFormat; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        /// Waits for the frame slot, acquires an image and begins the render pass, cleared to `clear`. Returns the
        /// command buffer to record into, or null when there is nothing to render into (minimized, closed).
        VkCommandBuffer BeginFrame(Carbon::Color clear)
        {
            Frame& frame = m_Frames[m_FrameIndex];
            // The GPU is done with the frame that used this slot FramesInFlight frames ago. Carbon relies on this.
            vkWaitForFences(m_Device, 1, &frame.Fence, VK_TRUE, UINT64_MAX);

            m_ImageIndex = 0;
            if (m_Window != nullptr)
            {
                const VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, frame.ImageAvailable,
                                                              VK_NULL_HANDLE, &m_ImageIndex);
                if (result == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    RecreateSwapchain();
                    return VK_NULL_HANDLE;
                }
                if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
                    return VK_NULL_HANDLE;
            }
            vkResetFences(m_Device, 1, &frame.Fence);

            vkResetCommandBuffer(frame.Commands, 0);
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(frame.Commands, &beginInfo);

            VkClearValue clearValue = {};
            clearValue.color = {{clear.R, clear.G, clear.B, 1.0f}};
            VkRenderPassBeginInfo passInfo = {};
            passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            passInfo.renderPass = m_RenderPass;
            passInfo.framebuffer = m_Targets[m_ImageIndex].Framebuffer;
            passInfo.renderArea = {{0, 0}, {m_Width, m_Height}};
            passInfo.clearValueCount = 1;
            passInfo.pClearValues = &clearValue;
            vkCmdBeginRenderPass(frame.Commands, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
            return frame.Commands;
        }

        /// Ends the render pass, submits and presents. With `readBack`, also copies the image into a buffer that
        /// SaveScreenshot reads.
        void EndFrame(bool readBack)
        {
            Frame& frame = m_Frames[m_FrameIndex];
            vkCmdEndRenderPass(frame.Commands);
            if (readBack)
            {
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
                if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || HasResized())
                    RecreateSwapchain();
            }
            m_FrameIndex = (m_FrameIndex + 1) % FramesInFlight;
        }

        /// Writes the image read back by the last EndFrame(true) as PNG.
        bool SaveScreenshot(const Example::Arguments& arguments, float contentScale)
        {
            vkDeviceWaitIdle(m_Device);
            void* pixels = nullptr;
            vkMapMemory(m_Device, m_ReadbackMemory, 0, VK_WHOLE_SIZE, 0, &pixels);
            const bool isSaved = Example::SaveScreenshot(arguments, static_cast<const uint8_t*>(pixels), m_Width,
                                                         m_Height, m_Width * 4, contentScale);
            vkUnmapMemory(m_Device, m_ReadbackMemory);
            return isSaved;
        }

        /// Draws the host's triangle into `area` (pixels) of the render pass.
        void DrawTriangle(VkCommandBuffer commands, const Carbon::Rect& area, float brightness) const
        {
            if (area.IsEmpty())
                return;
            vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, m_TrianglePipeline);
            const VkViewport viewport = {area.X, area.Y, area.Width, area.Height, 0.0f, 1.0f};
            vkCmdSetViewport(commands, 0, 1, &viewport);
            const VkRect2D scissor = {{static_cast<int32_t>(area.X), static_cast<int32_t>(area.Y)},
                                      {static_cast<uint32_t>(area.Width), static_cast<uint32_t>(area.Height)}};
            vkCmdSetScissor(commands, 0, 1, &scissor);
            vkCmdPushConstants(commands, m_TriangleLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &brightness);
            vkCmdDraw(commands, 3, 1, 0, 0);
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
            application.pApplicationName = "VulkanMinimalIntegration";
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
            return m_Window == nullptr ||
                   Check(glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface), "glfwCreateWindowSurface");
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
                std::fprintf(stderr, "No Vulkan device can render%s\n", m_Window != nullptr ? " to the window" : "");
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
            return Check(vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass), "vkCreateRenderPass");
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
                return true; // minimized; RecreateSwapchain tries again

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
            if (!Check(vkCreateSwapchainKHR(m_Device, &swapchainInfo, nullptr, &m_Swapchain), "vkCreateSwapchainKHR") ||
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

        bool CreateTrianglePipeline()
        {
            VkPushConstantRange range = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)};
            VkPipelineLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &range;
            vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_TriangleLayout);

            VkShaderModuleCreateInfo moduleInfo = {};
            moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            moduleInfo.codeSize = sizeof(TriangleVertexCode);
            moduleInfo.pCode = TriangleVertexCode;
            VkShaderModule vertex = VK_NULL_HANDLE;
            vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &vertex);
            moduleInfo.codeSize = sizeof(TriangleFragmentCode);
            moduleInfo.pCode = TriangleFragmentCode;
            VkShaderModule fragment = VK_NULL_HANDLE;
            vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &fragment);

            VkPipelineShaderStageCreateInfo stages[2] = {};
            stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = vertex;
            stages[0].pName = "main";
            stages[1] = stages[0];
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = fragment;
            VkPipelineVertexInputStateCreateInfo vertexInput = {};
            vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport = {};
            viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport.viewportCount = 1;
            viewport.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo rasterization = {};
            rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterization.cullMode = VK_CULL_MODE_NONE;
            rasterization.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo multisample = {};
            multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState blendAttachment = {};
            blendAttachment.colorWriteMask = 0xF;
            VkPipelineColorBlendStateCreateInfo blend = {};
            blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            blend.attachmentCount = 1;
            blend.pAttachments = &blendAttachment;
            const VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic = {};
            dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dynamicStates;

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = stages;
            pipelineInfo.pVertexInputState = &vertexInput;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewport;
            pipelineInfo.pRasterizationState = &rasterization;
            pipelineInfo.pMultisampleState = &multisample;
            pipelineInfo.pColorBlendState = &blend;
            pipelineInfo.pDynamicState = &dynamic;
            pipelineInfo.layout = m_TriangleLayout;
            pipelineInfo.renderPass = m_RenderPass;
            const bool isCreated = Check(
                vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_TrianglePipeline),
                "vkCreateGraphicsPipelines");
            vkDestroyShaderModule(m_Device, vertex, nullptr);
            vkDestroyShaderModule(m_Device, fragment, nullptr);
            return isCreated;
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

        bool HasResized() const
        {
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(m_Window, &width, &height);
            return static_cast<uint32_t>(width) != m_Width || static_cast<uint32_t>(height) != m_Height;
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
        VkPipelineLayout m_TriangleLayout = VK_NULL_HANDLE;
        VkPipeline m_TrianglePipeline = VK_NULL_HANDLE;
        VkBuffer m_Readback = VK_NULL_HANDLE;
        VkDeviceMemory m_ReadbackMemory = VK_NULL_HANDLE;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

    /// The swapchain's format in Carbon's terms.
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

    // ---------------------------------------------------------------------------------------------------------
    // The interface. It is rebuilt from this code every frame; the application owns all of its state.
    // ---------------------------------------------------------------------------------------------------------
    struct Settings
    {
        bool IsDark = false;
        bool ShowTriangle = true;
        float Brightness = 1.0f;
        std::string Name = "Triangle";
        int Saves = 0;
        // Where the host draws its own content this frame, in points. The interface reserves the space.
        Carbon::Rect HostArea;
    };

    void BuildInterface(Settings& settings)
    {
        using namespace Carbon;

        BeginHStack({.Spacing = 24.0f,
                     .Padding = 24.0f,
                     .Alignment = VerticalAlignment::Top,
                     .Width = Size::Fill(),
                     .Height = Size::Fill()});

        BeginVStack({.Spacing = 12.0f, .Width = 300.0f, .Height = Size::Fill()});
        Text("Settings", {.Style = TextStyle::LargeTitle, .Emphasized = true});
        Text("Carbon's interface and the host's triangle are drawn into the same Vulkan render pass.",
             {.Secondary = true, .Width = Size::Fill(), .Wraps = true});
        Spacer({.Length = 4.0f});

        if (Toggle("Dark Mode", &settings.IsDark, {.Width = Size::Fill()}))
            SetTheme(settings.IsDark ? Theme::Dark() : Theme::Light());
        Separator();
        Toggle("Show Triangle", &settings.ShowTriangle, {.Width = Size::Fill()});
        Separator();

        BeginHStack({.Spacing = 10.0f, .Width = Size::Fill()});
        Text("Brightness");
        Slider("Brightness", &settings.Brightness, 0.2f, 1.0f,
               {.Width = Size::Fill(), .Disabled = !settings.ShowTriangle});
        EndHStack();

        BeginHStack({.Spacing = 10.0f, .Width = Size::Fill()});
        Text("Name");
        TextField("Name", &settings.Name, {.Width = Size::Fill()});
        EndHStack();

        Spacer();
        BeginHStack({.Spacing = 8.0f, .Width = Size::Fill()});
        Text(settings.Saves == 0 ? std::string("Not saved yet") : "Saved " + std::to_string(settings.Saves) + " times",
             {.Style = TextStyle::Subheadline, .Secondary = true});
        Spacer();
        if (Button("Reset"))
        {
            settings.Brightness = 1.0f;
            settings.Name = "Triangle";
        }
        if (Button("Save", {.Role = ButtonRole::Prominent, .IsDefault = true}))
            settings.Saves++;
        EndHStack();
        EndVStack();

        BeginVStack({.Spacing = 8.0f, .Width = Size::Fill(), .Height = Size::Fill()});
        Text(std::string(Icons::Cube) + "  " + settings.Name, {.Style = TextStyle::Headline});
        const Rect frame = AllocateItem(Vec2(), {.Width = Size::Fill(), .Height = Size::Fill()});
        GetDrawList().AddSquircleStroke(frame, GetStyleColor(StyleColor::Separator), 14.0f, 1.0f);
        settings.HostArea = settings.ShowTriangle ? frame.Inset(EdgeInsets(24.0f)) : Rect();
        Text("Drawn by the host in the same render pass", {.Style = TextStyle::Caption1, .Secondary = true});
        EndVStack();

        EndHStack();
    }
} // namespace

int main(int argc, char** argv)
{
    const Example::Arguments arguments = Example::ParseArguments(argc, argv);
    const bool isScreenshot = !arguments.ScreenshotPath.empty();
    const int width = arguments.Width > 0 ? arguments.Width : 760;
    const int height = arguments.Height > 0 ? arguments.Height : 440;

    // ---- 0. The host's window and Vulkan device ----------------------------------------------------------------
    // Screenshot mode renders offscreen at a fixed scale and never opens a window.
    float contentScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
    GLFWwindow* window = nullptr;
    if (!isScreenshot)
    {
        if (!glfwInit() || !glfwVulkanSupported())
        {
            std::fprintf(stderr, "GLFW could not be initialized with Vulkan support\n");
            return 1;
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, arguments.Scale > 0.0f ? GLFW_FALSE : GLFW_TRUE);
        window = glfwCreateWindow(static_cast<int>(std::lround(width * contentScale)),
                                  static_cast<int>(std::lround(height * contentScale)),
                                  "Carbon - Vulkan Minimal Integration", nullptr, nullptr);
        if (window == nullptr)
            return 1;
    }
    int exitCode = 0;
    {
        // The host's Vulkan objects, the surface included, go before the window.
        VulkanHost host;
        if (!host.Create(window, static_cast<uint32_t>(std::lround(width * contentScale)),
                         static_cast<uint32_t>(std::lround(height * contentScale))))
            return 1;

        // ---- 1. Create the Carbon context and install the Vulkan backend -------------------------------------
        Carbon::ContextDescription description;
        description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message)
        {
            const std::string_view levelName = Carbon::ToString(level);
            std::fprintf(stderr, "[%.*s] %.*s: %.*s\n", static_cast<int>(levelName.size()), levelName.data(),
                         static_cast<int>(source.size()), source.data(), static_cast<int>(message.size()),
                         message.data());
        };
        Example::InstallPlatformCallbacks(window, description.Callbacks); // clipboard and cursor
        Carbon::Context* context = Carbon::CreateContext(description);

        Carbon::VulkanInitInfo info;
        info.Instance = host.GetInstance();
        info.PhysicalDevice = host.GetPhysicalDevice();
        info.Device = host.GetDevice();
        info.Queue = host.GetQueue();
        info.QueueFamily = host.GetQueueFamily();
        info.FramesInFlight = FramesInFlight;
        info.RenderPass = host.GetRenderPass(); // Carbon draws in the host's render pass, subpass 0
        info.ColorFormat = ToCarbonFormat(host.GetColorFormat());
        if (!Carbon::VulkanInit(info))
            return 1;

        Settings settings;
        settings.IsDark = arguments.IsDark;
        Carbon::SetTheme(settings.IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());

        // ---- 2. Forward the window's input events to Carbon --------------------------------------------------
        // WebGPUMinimalIntegration writes these callbacks out one by one; this is the same code.
        Example::InstallInputCallbacks(window);

        // ---- 3. The frame loop -------------------------------------------------------------------------------
        double lastTime = window != nullptr ? glfwGetTime() : 0.0;
        for (int frameIndex = 0;; frameIndex++)
        {
            float deltaTime = Example::ScreenshotDeltaTime;
            if (window != nullptr)
            {
                glfwPollEvents();
                if (glfwWindowShouldClose(window))
                    break;
                int framebufferWidth = 0;
                int framebufferHeight = 0;
                glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
                if (framebufferWidth == 0 || framebufferHeight == 0)
                {
                    glfwWaitEvents(); // minimized
                    continue;
                }
                float scaleX = 1.0f;
                float scaleY = 1.0f;
                glfwGetWindowContentScale(window, &scaleX, &scaleY);
                contentScale = arguments.Scale > 0.0f ? arguments.Scale : scaleX;
                const double now = glfwGetTime();
                deltaTime = static_cast<float>(now - lastTime);
                lastTime = now;
            }
            else if (frameIndex == Example::ScreenshotWarmupFrames)
            {
                break;
            }

            // Display metrics and timing, every frame, before NewFrame. The swapchain's size changes only when it
            // is recreated, in BeginFrame or EndFrame below.
            Carbon::IO& io = Carbon::GetIO();
            io.SetDisplaySize(static_cast<float>(host.GetWidth()) / contentScale,
                              static_cast<float>(host.GetHeight()) / contentScale);
            io.SetContentScale(contentScale);
            io.SetDeltaTime(deltaTime);

            Carbon::NewFrame();
            BuildInterface(settings);
            Carbon::EndFrame();

            // The render pass belongs to the host. It clears to the theme's background (which glides during a
            // theme switch), draws its own content, then Carbon's interface on top.
            const Carbon::Color background = Carbon::GetStyleColor(Carbon::StyleColor::Background);
            const VkCommandBuffer commands = host.BeginFrame(background);
            if (commands == VK_NULL_HANDLE)
                continue; // the swapchain was out of date; the next frame renders
            const Carbon::Rect area = settings.HostArea;
            host.DrawTriangle(
                commands,
                Carbon::Rect(std::round(area.X * contentScale), std::round(area.Y * contentScale),
                             std::round(area.Width * contentScale), std::round(area.Height * contentScale)),
                settings.Brightness);
            Carbon::VulkanRender(commands);

            const bool isLastScreenshotFrame = isScreenshot && frameIndex == Example::ScreenshotWarmupFrames - 1;
            host.EndFrame(isLastScreenshotFrame);
            if (isLastScreenshotFrame && !host.SaveScreenshot(arguments, contentScale))
                exitCode = 1;
        }

        // ---- 4. Shut down: the GPU must be done with Carbon's frames -----------------------------------------
        vkDeviceWaitIdle(host.GetDevice());
        Carbon::VulkanShutdown();
        Carbon::DestroyContext(context);
    }

    if (window != nullptr)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    return exitCode;
}
