#include "Carbon/Backends/Vulkan/VulkanBackend.h"

#include <memory>

#include "Carbon/Backends/Vulkan/VulkanRendererInternal.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon
{
    namespace
    {
        VkFormat ToVulkan(TextureFormat format)
        {
            switch (format)
            {
                case TextureFormat::Undefined:
                    return VK_FORMAT_UNDEFINED;
                case TextureFormat::RGBA8Unorm:
                    return VK_FORMAT_R8G8B8A8_UNORM;
                case TextureFormat::RGBA8UnormSrgb:
                    return VK_FORMAT_R8G8B8A8_SRGB;
                case TextureFormat::BGRA8Unorm:
                    return VK_FORMAT_B8G8R8A8_UNORM;
                case TextureFormat::BGRA8UnormSrgb:
                    return VK_FORMAT_B8G8R8A8_SRGB;
                case TextureFormat::RGB10A2Unorm:
                    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
                case TextureFormat::RGBA16Float:
                    return VK_FORMAT_R16G16B16A16_SFLOAT;
                case TextureFormat::Depth16Unorm:
                    return VK_FORMAT_D16_UNORM;
                case TextureFormat::Depth24Unorm:
                    return VK_FORMAT_X8_D24_UNORM_PACK32;
                case TextureFormat::Depth24UnormStencil8:
                    return VK_FORMAT_D24_UNORM_S8_UINT;
                case TextureFormat::Depth32Float:
                    return VK_FORMAT_D32_SFLOAT;
                case TextureFormat::Depth32FloatStencil8:
                    return VK_FORMAT_D32_SFLOAT_S8_UINT;
                case TextureFormat::Stencil8:
                    return VK_FORMAT_S8_UINT;
            }
            return VK_FORMAT_UNDEFINED;
        }

        Internal::VulkanRenderer* GetRenderer(std::string_view function)
        {
            Internal::VulkanRenderer* renderer = GetRendererBackend<Internal::VulkanRenderer>();
            CB_VERIFY(renderer != nullptr, "{} needs the Vulkan backend; call VulkanInit first", function);
            return renderer;
        }

        bool IsValidSampleCount(uint32_t count)
        {
            return count >= 1 && count <= 64 && (count & (count - 1)) == 0;
        }
    } // namespace

    bool VulkanInit(const VulkanInitInfo& info)
    {
        if (info.PhysicalDevice == VK_NULL_HANDLE || info.Device == VK_NULL_HANDLE || info.Queue == VK_NULL_HANDLE)
        {
            CB_LOG_ERROR("Vulkan", "VulkanInit needs a physical device, a device and a queue");
            return false;
        }
        if (!IsColorFormat(info.ColorFormat))
        {
            CB_LOG_ERROR("Vulkan", "Unsupported color format {}", ToString(info.ColorFormat));
            return false;
        }
        if (info.DepthStencilFormat != TextureFormat::Undefined && !IsDepthStencilFormat(info.DepthStencilFormat))
        {
            CB_LOG_ERROR("Vulkan", "Unsupported depth-stencil format {}", ToString(info.DepthStencilFormat));
            return false;
        }
        if (!IsValidSampleCount(info.SampleCount))
        {
            CB_LOG_ERROR("Vulkan", "Invalid sample count {}", info.SampleCount);
            return false;
        }

        const VkFormat depthStencil = ToVulkan(info.DepthStencilFormat);
        const VkFormat depthFormat = HasDepth(info.DepthStencilFormat) ? depthStencil : VK_FORMAT_UNDEFINED;
        const VkFormat stencilFormat = HasStencil(info.DepthStencilFormat) ? depthStencil : VK_FORMAT_UNDEFINED;
        std::unique_ptr<Internal::VulkanRenderer> renderer =
            std::make_unique<Internal::VulkanRenderer>(info, ToVulkan(info.ColorFormat), depthFormat, stencilFormat);
        if (!renderer->IsValid())
        {
            CB_LOG_ERROR("Vulkan", "The Vulkan backend could not be created");
            return false;
        }
        return InstallRendererBackend(std::move(renderer));
    }

    void VulkanShutdown()
    {
        if (GetRendererBackend<Internal::VulkanRenderer>() != nullptr)
            RemoveRendererBackend();
    }

    void VulkanRender(VkCommandBuffer commandBuffer)
    {
        Internal::VulkanRenderer* renderer = GetRenderer("VulkanRender");
        if (renderer == nullptr)
            return;
        renderer->SetCommandBuffer(commandBuffer);
        RenderDrawData();
        renderer->SetCommandBuffer(VK_NULL_HANDLE);
    }

    TextureID VulkanGetTextureID(VkImageView view, VkImageLayout layout)
    {
        Internal::VulkanRenderer* renderer = GetRenderer("VulkanGetTextureID");
        return renderer != nullptr ? renderer->RegisterTexture(view, layout) : TextureID();
    }

    void VulkanReleaseTexture(VkImageView view)
    {
        if (GetRenderer("VulkanReleaseTexture") != nullptr && view != VK_NULL_HANDLE)
            ReleaseHostTexture(Internal::VulkanRenderer::GetTextureKey(view));
    }

    void VulkanImage(VkImageView view, Vec2 size, const ImageOptions& options, VkImageLayout layout)
    {
        Image(VulkanGetTextureID(view, layout), size, options);
    }
} // namespace Carbon
