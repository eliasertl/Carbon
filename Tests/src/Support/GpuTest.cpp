#include "Support/GpuTest.h"

#include <cmath>
#include <cstring>

namespace Carbon
{
    std::vector<std::string> GpuTest::s_DeviceErrors;

    void GpuTest::SetUp()
    {
        s_DeviceErrors.clear();

        static const wgpu::InstanceFeatureName InstanceFeatures[] = {wgpu::InstanceFeatureName::TimedWaitAny};
        wgpu::InstanceDescriptor instanceDescriptor;
        instanceDescriptor.requiredFeatureCount = 1;
        instanceDescriptor.requiredFeatures = InstanceFeatures;
        m_Instance = wgpu::CreateInstance(&instanceDescriptor);
        if (m_Instance == nullptr)
            GTEST_SKIP() << "No WebGPU instance available";

        wgpu::RequestAdapterOptions adapterOptions;
        m_Instance.WaitAny(
            m_Instance.RequestAdapter(
                &adapterOptions, wgpu::CallbackMode::WaitAnyOnly,
                [](wgpu::RequestAdapterStatus, wgpu::Adapter adapter, wgpu::StringView, wgpu::Adapter* out)
                { *out = std::move(adapter); }, &m_Adapter),
            UINT64_MAX);
        if (m_Adapter == nullptr)
            GTEST_SKIP() << "No WebGPU adapter available on this machine";

        wgpu::DeviceDescriptor deviceDescriptor;
        deviceDescriptor.SetUncapturedErrorCallback([](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message)
                                                    { s_DeviceErrors.emplace_back(std::string_view(message)); });
        m_Instance.WaitAny(m_Adapter.RequestDevice(
                               &deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly,
                               [](wgpu::RequestDeviceStatus, wgpu::Device device, wgpu::StringView, wgpu::Device* out)
                               { *out = std::move(device); }, &m_Device),
                           UINT64_MAX);
        if (m_Device == nullptr)
            GTEST_SKIP() << "No WebGPU device could be created";

        ContextDescription description;
        description.Device = m_Device;
        description.ColorFormat = wgpu::TextureFormat::RGBA8Unorm;
        description.Callbacks.AssertFailed = [this](const AssertInfo& info)
        { m_AssertMessages.emplace_back(info.Message); };
        m_Context = CreateContext(description);
        SetCurrentContext(m_Context);
    }

    void GpuTest::TearDown()
    {
        if (m_Context != nullptr)
        {
            DestroyContext(m_Context);
            m_Context = nullptr;
            m_Instance.ProcessEvents();
            EXPECT_TRUE(s_DeviceErrors.empty()) << "WebGPU error: " << s_DeviceErrors.front();
            EXPECT_TRUE(m_AssertMessages.empty());
        }
        m_Device = nullptr;
        m_Adapter = nullptr;
        m_Instance = nullptr;
    }

    RenderedImage GpuTest::RenderFrame(float width, float height, float contentScale, Color background,
                                       const std::function<void(DrawList&)>& build)
    {
        RenderedImage image;
        image.Width = static_cast<uint32_t>(std::lround(width * contentScale));
        image.Height = static_cast<uint32_t>(std::lround(height * contentScale));

        IO& io = GetIO();
        io.SetDisplaySize(width, height);
        io.SetContentScale(contentScale);
        io.SetDeltaTime(1.0f / 60.0f);
        NewFrame();
        build(GetDrawList());
        EndFrame();

        wgpu::TextureDescriptor textureDescriptor;
        textureDescriptor.size = {image.Width, image.Height, 1};
        textureDescriptor.format = wgpu::TextureFormat::RGBA8Unorm;
        textureDescriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
        const wgpu::Texture target = m_Device.CreateTexture(&textureDescriptor);

        wgpu::RenderPassColorAttachment colorAttachment;
        colorAttachment.view = target.CreateView();
        colorAttachment.loadOp = wgpu::LoadOp::Clear;
        colorAttachment.storeOp = wgpu::StoreOp::Store;
        colorAttachment.clearValue = {background.R, background.G, background.B, background.A};
        wgpu::RenderPassDescriptor passDescriptor;
        passDescriptor.colorAttachmentCount = 1;
        passDescriptor.colorAttachments = &colorAttachment;

        const uint32_t bytesPerRow = (image.Width * 4 + 255) & ~255u;
        const uint64_t bufferSize = static_cast<uint64_t>(bytesPerRow) * image.Height;
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        const wgpu::Buffer buffer = m_Device.CreateBuffer(&bufferDescriptor);

        const wgpu::CommandEncoder encoder = m_Device.CreateCommandEncoder();
        const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
        Render(pass);
        pass.End();

        wgpu::TexelCopyTextureInfo source;
        source.texture = target;
        wgpu::TexelCopyBufferInfo destination;
        destination.buffer = buffer;
        destination.layout.bytesPerRow = bytesPerRow;
        destination.layout.rowsPerImage = image.Height;
        const wgpu::Extent3D extent = {image.Width, image.Height, 1};
        encoder.CopyTextureToBuffer(&source, &destination, &extent);
        const wgpu::CommandBuffer commands = encoder.Finish();
        m_Device.GetQueue().Submit(1, &commands);

        bool isMapped = false;
        m_Instance.WaitAny(buffer.MapAsync(
                               wgpu::MapMode::Read, 0, static_cast<size_t>(bufferSize), wgpu::CallbackMode::WaitAnyOnly,
                               [](wgpu::MapAsyncStatus status, wgpu::StringView, bool* result)
                               { *result = status == wgpu::MapAsyncStatus::Success; }, &isMapped),
                           UINT64_MAX);
        EXPECT_TRUE(isMapped);
        if (!isMapped)
            return image;

        const uint8_t* mapped =
            static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, static_cast<size_t>(bufferSize)));
        image.Pixels.resize(static_cast<size_t>(image.Width) * image.Height * 4);
        for (uint32_t row = 0; row < image.Height; row++)
        {
            std::memcpy(&image.Pixels[static_cast<size_t>(row) * image.Width * 4],
                        mapped + static_cast<size_t>(row) * bytesPerRow, static_cast<size_t>(image.Width) * 4);
        }
        buffer.Unmap();
        return image;
    }
} // namespace Carbon
