#include "Support/BackendHarness.h"

#if defined(CARBON_HAS_BACKEND_WEBGPU)

#include <cstring>
#include <format>

#include <webgpu/webgpu_cpp.h>

#include "Carbon/Backends/WebGPU/WebGPUBackend.h"

namespace Carbon
{
    namespace
    {
        /// Errors reported by the device's uncaptured-error callback. Tests run one at a time.
        std::vector<std::string> s_DeviceErrors;

        wgpu::TextureFormat ToWebGPU(TextureFormat format)
        {
            switch (format)
            {
                case TextureFormat::RGBA8UnormSrgb:
                    return wgpu::TextureFormat::RGBA8UnormSrgb;
                case TextureFormat::BGRA8Unorm:
                    return wgpu::TextureFormat::BGRA8Unorm;
                case TextureFormat::BGRA8UnormSrgb:
                    return wgpu::TextureFormat::BGRA8UnormSrgb;
                default:
                    return wgpu::TextureFormat::RGBA8Unorm;
            }
        }

        class WebGPUHarness : public BackendHarness
        {
        public:
            ~WebGPUHarness() override
            {
                m_Textures.clear();
                m_Device = nullptr;
                m_Adapter = nullptr;
                m_Instance = nullptr;
            }

            std::string_view GetName() const override { return "WebGPU"; }

            std::string CreateDevice() override
            {
                s_DeviceErrors.clear();
                static const wgpu::InstanceFeatureName InstanceFeatures[] = {wgpu::InstanceFeatureName::TimedWaitAny};
                wgpu::InstanceDescriptor instanceDescriptor;
                instanceDescriptor.requiredFeatureCount = 1;
                instanceDescriptor.requiredFeatures = InstanceFeatures;
                m_Instance = wgpu::CreateInstance(&instanceDescriptor);
                if (m_Instance == nullptr)
                    return "No WebGPU instance available";

                wgpu::RequestAdapterOptions adapterOptions;
                m_Instance.WaitAny(
                    m_Instance.RequestAdapter(
                        &adapterOptions, wgpu::CallbackMode::WaitAnyOnly,
                        [](wgpu::RequestAdapterStatus, wgpu::Adapter adapter, wgpu::StringView, wgpu::Adapter* out)
                        { *out = std::move(adapter); }, &m_Adapter),
                    UINT64_MAX);
                if (m_Adapter == nullptr)
                    return "No WebGPU adapter available on this machine";

                wgpu::DeviceDescriptor deviceDescriptor;
                deviceDescriptor.SetUncapturedErrorCallback(
                    [](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message)
                    { s_DeviceErrors.emplace_back(std::string_view(message)); });
                m_Instance.WaitAny(
                    m_Adapter.RequestDevice(
                        &deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly,
                        [](wgpu::RequestDeviceStatus, wgpu::Device device, wgpu::StringView, wgpu::Device* out)
                        { *out = std::move(device); }, &m_Device),
                    UINT64_MAX);
                if (m_Device == nullptr)
                    return "No WebGPU device could be created";
                return {};
            }

            bool InitBackend(TextureFormat colorFormat) override
            {
                m_ColorFormat = ToWebGPU(colorFormat);
                WebGPUInitInfo info;
                info.Device = m_Device;
                info.ColorFormat = colorFormat;
                return WebGPUInit(info);
            }

            void ShutdownBackend() override { WebGPUShutdown(); }

            RenderedImage RenderFrame(uint32_t width, uint32_t height, Color background) override
            {
                RenderedImage image;
                image.Width = width;
                image.Height = height;

                wgpu::TextureDescriptor textureDescriptor;
                textureDescriptor.size = {width, height, 1};
                textureDescriptor.format = m_ColorFormat;
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

                const uint32_t bytesPerRow = (width * 4 + 255) & ~255u;
                const uint64_t bufferSize = static_cast<uint64_t>(bytesPerRow) * height;
                wgpu::BufferDescriptor bufferDescriptor;
                bufferDescriptor.size = bufferSize;
                bufferDescriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
                const wgpu::Buffer buffer = m_Device.CreateBuffer(&bufferDescriptor);

                const wgpu::CommandEncoder encoder = m_Device.CreateCommandEncoder();
                const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
                WebGPURender(pass);
                pass.End();

                wgpu::TexelCopyTextureInfo source;
                source.texture = target;
                wgpu::TexelCopyBufferInfo destination;
                destination.buffer = buffer;
                destination.layout.bytesPerRow = bytesPerRow;
                destination.layout.rowsPerImage = height;
                const wgpu::Extent3D extent = {width, height, 1};
                encoder.CopyTextureToBuffer(&source, &destination, &extent);
                const wgpu::CommandBuffer commands = encoder.Finish();
                m_Device.GetQueue().Submit(1, &commands);

                bool isMapped = false;
                m_Instance.WaitAny(
                    buffer.MapAsync(
                        wgpu::MapMode::Read, 0, static_cast<size_t>(bufferSize), wgpu::CallbackMode::WaitAnyOnly,
                        [](wgpu::MapAsyncStatus status, wgpu::StringView, bool* result)
                        { *result = status == wgpu::MapAsyncStatus::Success; }, &isMapped),
                    UINT64_MAX);
                if (!isMapped)
                {
                    s_DeviceErrors.emplace_back("Could not map the readback buffer");
                    return image;
                }

                const uint8_t* mapped =
                    static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, static_cast<size_t>(bufferSize)));
                image.Pixels.resize(static_cast<size_t>(width) * height * 4);
                const bool isBgra = m_ColorFormat == wgpu::TextureFormat::BGRA8Unorm ||
                                    m_ColorFormat == wgpu::TextureFormat::BGRA8UnormSrgb;
                for (uint32_t row = 0; row < height; row++)
                {
                    uint8_t* out = &image.Pixels[static_cast<size_t>(row) * width * 4];
                    std::memcpy(out, mapped + static_cast<size_t>(row) * bytesPerRow, static_cast<size_t>(width) * 4);
                    if (isBgra)
                    {
                        for (uint32_t x = 0; x < width; x++)
                            std::swap(out[x * 4], out[x * 4 + 2]);
                    }
                }
                buffer.Unmap();
                return image;
            }

            size_t CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
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
                return m_Textures.size() - 1;
            }

            TextureID GetTextureID(size_t texture) override { return WebGPUGetTextureID(m_Textures[texture]); }

            std::vector<std::string> TakeMessages() override
            {
                if (m_Instance != nullptr)
                    m_Instance.ProcessEvents();
                std::vector<std::string> messages = std::move(s_DeviceErrors);
                s_DeviceErrors.clear();
                return messages;
            }

        private:
            wgpu::Instance m_Instance;
            wgpu::Adapter m_Adapter;
            wgpu::Device m_Device;
            wgpu::TextureFormat m_ColorFormat = wgpu::TextureFormat::RGBA8Unorm;
            std::vector<wgpu::TextureView> m_Textures;
        };
    } // namespace

    std::unique_ptr<BackendHarness> CreateWebGPUHarness()
    {
        return std::make_unique<WebGPUHarness>();
    }
} // namespace Carbon

#endif
