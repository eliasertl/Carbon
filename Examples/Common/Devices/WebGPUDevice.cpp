#include "WebGPUDevice.h"

#include <cstdio>
#include <cstring>
#include <string_view>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "WebGPUSurface.h"

namespace Example
{
    namespace
    {
        void PrintString(const char* prefix, wgpu::StringView message)
        {
            const std::string_view view = message;
            std::fprintf(stderr, "%s%.*s\n", prefix, static_cast<int>(view.size()), view.data());
        }
    } // namespace

    std::unique_ptr<GraphicsDevice> CreateGraphicsDevice()
    {
        return std::make_unique<WebGPUDevice>();
    }

    void WebGPUDevice::SetWindowHints() const
    {
        // Carbon renders through WebGPU; GLFW must not create an OpenGL context.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    bool WebGPUDevice::Create(GLFWwindow* window, bool isOffscreen, uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;
        static const wgpu::InstanceFeatureName InstanceFeatures[] = {wgpu::InstanceFeatureName::TimedWaitAny};
        wgpu::InstanceDescriptor instanceDescriptor;
        instanceDescriptor.requiredFeatureCount = 1;
        instanceDescriptor.requiredFeatures = InstanceFeatures;
        m_Instance = wgpu::CreateInstance(&instanceDescriptor);
        if (m_Instance == nullptr)
        {
            std::fprintf(stderr, "Could not create a WebGPU instance\n");
            return false;
        }

        if (!isOffscreen)
        {
            m_Surface = CreateSurfaceForWindow(m_Instance, window);
            if (m_Surface == nullptr)
            {
                std::fprintf(stderr, "Could not create a surface for the window\n");
                return false;
            }
        }

        wgpu::RequestAdapterOptions adapterOptions;
        adapterOptions.compatibleSurface = m_Surface;
        adapterOptions.powerPreference = wgpu::PowerPreference::HighPerformance;
        m_Instance.WaitAny(
            m_Instance.RequestAdapter(
                &adapterOptions, wgpu::CallbackMode::WaitAnyOnly,
                [](wgpu::RequestAdapterStatus, wgpu::Adapter adapter, wgpu::StringView, wgpu::Adapter* out)
                { *out = std::move(adapter); }, &m_Adapter),
            UINT64_MAX);
        if (m_Adapter == nullptr)
        {
            std::fprintf(stderr, "No WebGPU adapter is available on this machine\n");
            return false;
        }

        wgpu::DeviceDescriptor deviceDescriptor;
        deviceDescriptor.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message)
                                                    { PrintString("WebGPU error: ", message); });
        m_Instance.WaitAny(m_Adapter.RequestDevice(
                               &deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly,
                               [](wgpu::RequestDeviceStatus, wgpu::Device device, wgpu::StringView, wgpu::Device* out)
                               { *out = std::move(device); }, &m_Device),
                           UINT64_MAX);
        if (m_Device == nullptr)
        {
            std::fprintf(stderr, "Could not create a WebGPU device\n");
            return false;
        }

        if (isOffscreen)
        {
            // RGBA so the pixels can be written to a PNG without conversion.
            m_ColorFormat = wgpu::TextureFormat::RGBA8Unorm;
            wgpu::TextureDescriptor descriptor;
            descriptor.label = "Screenshot target";
            descriptor.size = {m_Width, m_Height, 1};
            descriptor.format = m_ColorFormat;
            descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
            m_OffscreenTexture = m_Device.CreateTexture(&descriptor);
            return true;
        }

        // Prefer a non-sRGB format: Carbon then blends in gamma space, like macOS UI.
        wgpu::SurfaceCapabilities capabilities;
        m_Surface.GetCapabilities(m_Adapter, &capabilities);
        if (capabilities.formatCount == 0)
        {
            std::fprintf(stderr, "The surface reports no supported formats\n");
            return false;
        }
        m_ColorFormat = capabilities.formats[0];
        for (size_t i = 0; i < capabilities.formatCount; i++)
        {
            if (capabilities.formats[i] == wgpu::TextureFormat::BGRA8Unorm ||
                capabilities.formats[i] == wgpu::TextureFormat::RGBA8Unorm)
            {
                m_ColorFormat = capabilities.formats[i];
                break;
            }
        }
        return true;
    }

    bool WebGPUDevice::InitCarbon()
    {
        Carbon::WebGPUInitInfo info;
        info.Device = m_Device;
        info.ColorFormat = GetCarbonColorFormat();
        return Carbon::WebGPUInit(info);
    }

    void WebGPUDevice::ShutdownCarbon()
    {
        Carbon::WebGPUShutdown();
    }

    void WebGPUDevice::ConfigureSurface()
    {
        wgpu::SurfaceConfiguration configuration;
        configuration.device = m_Device;
        configuration.format = m_ColorFormat;
        configuration.width = m_Width;
        configuration.height = m_Height;
        configuration.presentMode = wgpu::PresentMode::Fifo;
        m_Surface.Configure(&configuration);
        m_ConfiguredWidth = m_Width;
        m_ConfiguredHeight = m_Height;
    }

    bool WebGPUDevice::BeginFrame(uint32_t width, uint32_t height)
    {
        if (m_Surface == nullptr)
        {
            m_TargetView = m_OffscreenTexture.CreateView();
            return true;
        }

        m_Width = width;
        m_Height = height;
        if (m_Width != m_ConfiguredWidth || m_Height != m_ConfiguredHeight)
            ConfigureSurface();
        wgpu::SurfaceTexture surfaceTexture;
        m_Surface.GetCurrentTexture(&surfaceTexture);
        if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
            surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
        {
            // The surface went stale (resize, display change): configure it again and retry.
            ConfigureSurface();
            m_Surface.GetCurrentTexture(&surfaceTexture);
            if (surfaceTexture.texture == nullptr)
                return false;
        }
        m_TargetView = surfaceTexture.texture.CreateView();
        return true;
    }

    void WebGPUDevice::Render(Carbon::Color background)
    {
        wgpu::RenderPassColorAttachment colorAttachment;
        colorAttachment.view = m_TargetView;
        colorAttachment.loadOp = wgpu::LoadOp::Clear;
        colorAttachment.storeOp = wgpu::StoreOp::Store;
        colorAttachment.clearValue = {background.R, background.G, background.B, 1.0};
        wgpu::RenderPassDescriptor passDescriptor;
        passDescriptor.colorAttachmentCount = 1;
        passDescriptor.colorAttachments = &colorAttachment;

        const wgpu::CommandEncoder encoder = m_Device.CreateCommandEncoder();
        const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
        Carbon::WebGPURender(pass);
        pass.End();
        const wgpu::CommandBuffer commands = encoder.Finish();
        m_Device.GetQueue().Submit(1, &commands);
    }

    void WebGPUDevice::EndFrame()
    {
        m_TargetView = nullptr;
        if (m_Surface != nullptr)
            m_Surface.Present();
        m_Instance.ProcessEvents();
    }

    bool WebGPUDevice::ReadPixels(std::vector<uint8_t>& pixels)
    {
        // Rows in a buffer copy must be aligned to 256 bytes.
        const uint32_t bytesPerRow = (m_Width * 4 + 255) & ~255u;
        const uint64_t bufferSize = static_cast<uint64_t>(bytesPerRow) * m_Height;

        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.label = "Screenshot readback";
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        const wgpu::Buffer buffer = m_Device.CreateBuffer(&bufferDescriptor);

        wgpu::TexelCopyTextureInfo source;
        source.texture = m_OffscreenTexture;
        wgpu::TexelCopyBufferInfo destination;
        destination.buffer = buffer;
        destination.layout.bytesPerRow = bytesPerRow;
        destination.layout.rowsPerImage = m_Height;
        const wgpu::Extent3D extent = {m_Width, m_Height, 1};

        const wgpu::CommandEncoder encoder = m_Device.CreateCommandEncoder();
        encoder.CopyTextureToBuffer(&source, &destination, &extent);
        const wgpu::CommandBuffer commands = encoder.Finish();
        m_Device.GetQueue().Submit(1, &commands);

        bool isMapped = false;
        m_Instance.WaitAny(buffer.MapAsync(
                               wgpu::MapMode::Read, 0, static_cast<size_t>(bufferSize), wgpu::CallbackMode::WaitAnyOnly,
                               [](wgpu::MapAsyncStatus status, wgpu::StringView, bool* result)
                               { *result = status == wgpu::MapAsyncStatus::Success; }, &isMapped),
                           UINT64_MAX);
        if (!isMapped)
            return false;

        const uint8_t* mapped =
            static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, static_cast<size_t>(bufferSize)));
        const size_t rowBytes = static_cast<size_t>(m_Width) * 4;
        pixels.resize(rowBytes * m_Height);
        for (uint32_t row = 0; row < m_Height; row++)
            std::memcpy(&pixels[row * rowBytes], mapped + static_cast<size_t>(row) * bytesPerRow, rowBytes);
        buffer.Unmap();
        return true;
    }

    Carbon::TextureID WebGPUDevice::CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels)
    {
        wgpu::TextureDescriptor descriptor;
        descriptor.size = {width, height, 1};
        descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
        descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
        const wgpu::Texture texture = m_Device.CreateTexture(&descriptor);
        wgpu::TexelCopyTextureInfo destination;
        destination.texture = texture;
        wgpu::TexelCopyBufferLayout layout;
        layout.bytesPerRow = width * 4;
        layout.rowsPerImage = height;
        const wgpu::Extent3D extent = {width, height, 1};
        m_Device.GetQueue().WriteTexture(&destination, texels.data(), texels.size(), &layout, &extent);
        m_Textures.push_back(texture.CreateView());
        return Carbon::MakeTextureID(m_Textures.back().Get());
    }

    Carbon::TextureFormat WebGPUDevice::GetCarbonColorFormat() const
    {
        switch (m_ColorFormat)
        {
            case wgpu::TextureFormat::RGBA8Unorm:
                return Carbon::TextureFormat::RGBA8Unorm;
            case wgpu::TextureFormat::RGBA8UnormSrgb:
                return Carbon::TextureFormat::RGBA8UnormSrgb;
            case wgpu::TextureFormat::BGRA8UnormSrgb:
                return Carbon::TextureFormat::BGRA8UnormSrgb;
            case wgpu::TextureFormat::RGB10A2Unorm:
                return Carbon::TextureFormat::RGB10A2Unorm;
            case wgpu::TextureFormat::RGBA16Float:
                return Carbon::TextureFormat::RGBA16Float;
            default:
                return Carbon::TextureFormat::BGRA8Unorm;
        }
    }
} // namespace Example
