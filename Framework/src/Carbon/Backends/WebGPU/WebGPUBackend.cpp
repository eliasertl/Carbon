#include "Carbon/Backends/WebGPU/WebGPUBackend.h"

#include <memory>

#include "Carbon/Backends/WebGPU/WebGPURendererInternal.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Renderer/RendererBackend.h"

namespace Carbon
{
    namespace
    {
        bool ToWebGPU(TextureFormat format, wgpu::TextureFormat& result)
        {
            switch (format)
            {
                case TextureFormat::Undefined:
                    result = wgpu::TextureFormat::Undefined;
                    return true;
                case TextureFormat::RGBA8Unorm:
                    result = wgpu::TextureFormat::RGBA8Unorm;
                    return true;
                case TextureFormat::RGBA8UnormSrgb:
                    result = wgpu::TextureFormat::RGBA8UnormSrgb;
                    return true;
                case TextureFormat::BGRA8Unorm:
                    result = wgpu::TextureFormat::BGRA8Unorm;
                    return true;
                case TextureFormat::BGRA8UnormSrgb:
                    result = wgpu::TextureFormat::BGRA8UnormSrgb;
                    return true;
                case TextureFormat::RGB10A2Unorm:
                    result = wgpu::TextureFormat::RGB10A2Unorm;
                    return true;
                case TextureFormat::RGBA16Float:
                    result = wgpu::TextureFormat::RGBA16Float;
                    return true;
                case TextureFormat::Depth16Unorm:
                    result = wgpu::TextureFormat::Depth16Unorm;
                    return true;
                case TextureFormat::Depth24Unorm:
                    result = wgpu::TextureFormat::Depth24Plus;
                    return true;
                case TextureFormat::Depth24UnormStencil8:
                    result = wgpu::TextureFormat::Depth24PlusStencil8;
                    return true;
                case TextureFormat::Depth32Float:
                    result = wgpu::TextureFormat::Depth32Float;
                    return true;
                case TextureFormat::Depth32FloatStencil8:
                    result = wgpu::TextureFormat::Depth32FloatStencil8;
                    return true;
                case TextureFormat::Stencil8:
                    result = wgpu::TextureFormat::Stencil8;
                    return true;
            }
            return false;
        }

        Internal::WebGPURenderer* GetRenderer(std::string_view function)
        {
            Internal::WebGPURenderer* renderer = GetRendererBackend<Internal::WebGPURenderer>();
            CB_VERIFY(renderer != nullptr, "{} needs the WebGPU backend; call WebGPUInit first", function);
            return renderer;
        }
    } // namespace

    bool WebGPUInit(const WebGPUInitInfo& info)
    {
        if (info.Device == nullptr)
        {
            CB_LOG_ERROR("WebGPU", "WebGPUInit needs a device");
            return false;
        }
        wgpu::TextureFormat colorFormat = wgpu::TextureFormat::Undefined;
        if (!IsColorFormat(info.ColorFormat) || !ToWebGPU(info.ColorFormat, colorFormat))
        {
            CB_LOG_ERROR("WebGPU", "Unsupported color format {}", ToString(info.ColorFormat));
            return false;
        }
        wgpu::TextureFormat depthStencilFormat = wgpu::TextureFormat::Undefined;
        if ((info.DepthStencilFormat != TextureFormat::Undefined && !IsDepthStencilFormat(info.DepthStencilFormat)) ||
            !ToWebGPU(info.DepthStencilFormat, depthStencilFormat))
        {
            CB_LOG_ERROR("WebGPU", "Unsupported depth-stencil format {}", ToString(info.DepthStencilFormat));
            return false;
        }
        return InstallRendererBackend(
            std::make_unique<Internal::WebGPURenderer>(info.Device, colorFormat, depthStencilFormat, info.SampleCount));
    }

    void WebGPUShutdown()
    {
        if (GetRendererBackend<Internal::WebGPURenderer>() != nullptr)
            RemoveRendererBackend();
    }

    void WebGPURender(const wgpu::RenderPassEncoder& pass)
    {
        Internal::WebGPURenderer* renderer = GetRenderer("WebGPURender");
        if (renderer == nullptr)
            return;
        renderer->SetRenderPass(pass);
        RenderDrawData();
        renderer->SetRenderPass(nullptr);
    }

    TextureID WebGPUGetTextureID(const wgpu::TextureView& view)
    {
        Internal::WebGPURenderer* renderer = GetRenderer("WebGPUGetTextureID");
        return renderer != nullptr ? renderer->RegisterTexture(view) : TextureID();
    }

    void WebGPUImage(const wgpu::TextureView& view, Vec2 size, const ImageOptions& options)
    {
        Image(WebGPUGetTextureID(view), size, options);
    }
} // namespace Carbon
