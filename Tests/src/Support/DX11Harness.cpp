#include "Support/BackendHarness.h"

#if defined(CARBON_HAS_BACKEND_DX11)

#include <cstring>
#include <format>

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl/client.h>

#include "Carbon/Backends/DX11/DX11Backend.h"

namespace Carbon
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        DXGI_FORMAT ToDXGI(TextureFormat format)
        {
            switch (format)
            {
                case TextureFormat::RGBA8UnormSrgb:
                    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                case TextureFormat::BGRA8Unorm:
                    return DXGI_FORMAT_B8G8R8A8_UNORM;
                case TextureFormat::BGRA8UnormSrgb:
                    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                default:
                    return DXGI_FORMAT_R8G8B8A8_UNORM;
            }
        }

        /// Drives the Direct3D 11 backend on a hardware device, or WARP when there is none, with the debug layer
        /// when it is installed. Its warnings and errors, and any pipeline state the backend fails to restore, are
        /// collected as messages.
        class DX11Harness : public BackendHarness
        {
        public:
            std::string_view GetName() const override { return "DX11"; }

            std::string CreateDevice() override
            {
                const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
                for (const D3D_DRIVER_TYPE driver : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP})
                {
                    // The debug layer is optional (Graphics Tools); try with it first.
                    for (const UINT flags : {static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG), 0u})
                    {
                        if (SUCCEEDED(D3D11CreateDevice(nullptr, driver, nullptr, flags, levels, 2, D3D11_SDK_VERSION,
                                                        &m_Device, nullptr, &m_Context)))
                        {
                            m_Device.As(&m_InfoQueue);
                            return {};
                        }
                    }
                }
                return "No Direct3D 11 device could be created";
            }

            bool InitBackend(TextureFormat colorFormat) override
            {
                m_Format = ToDXGI(colorFormat);
                DX11InitInfo info;
                info.Device = m_Device.Get();
                info.Context = m_Context.Get();
                info.ColorFormat = colorFormat;
                return DX11Init(info);
            }

            void ShutdownBackend() override { DX11Shutdown(); }

            RenderedImage RenderFrame(uint32_t width, uint32_t height, Color background) override
            {
                RenderedImage image;
                image.Width = width;
                image.Height = height;

                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = width;
                desc.Height = height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = m_Format;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_RENDER_TARGET;
                ComPtr<ID3D11Texture2D> target;
                ComPtr<ID3D11RenderTargetView> view;
                m_Device->CreateTexture2D(&desc, nullptr, &target);
                m_Device->CreateRenderTargetView(target.Get(), nullptr, &view);
                desc.Usage = D3D11_USAGE_STAGING;
                desc.BindFlags = 0;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                ComPtr<ID3D11Texture2D> staging;
                m_Device->CreateTexture2D(&desc, nullptr, &staging);

                const float clear[4] = {background.R, background.G, background.B, background.A};
                m_Context->ClearRenderTargetView(view.Get(), clear);
                ID3D11RenderTargetView* views[1] = {view.Get()};
                m_Context->OMSetRenderTargets(1, views, nullptr);

                // State a host might have left: DX11Render must restore it.
                const D3D11_VIEWPORT hostViewport = {5.0f, 6.0f, 7.0f, 8.0f, 0.0f, 1.0f};
                m_Context->RSSetViewports(1, &hostViewport);
                const D3D11_RECT hostScissor = {1, 2, 3, 4};
                m_Context->RSSetScissorRects(1, &hostScissor);
                m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
                {
                    const RenderTimer timer(*this);
                    DX11Render();
                }
                UINT count = 1;
                D3D11_VIEWPORT viewport = {};
                m_Context->RSGetViewports(&count, &viewport);
                D3D11_RECT scissor = {};
                count = 1;
                m_Context->RSGetScissorRects(&count, &scissor);
                D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
                m_Context->IAGetPrimitiveTopology(&topology);
                ComPtr<ID3D11PixelShader> shader;
                m_Context->PSGetShader(&shader, nullptr, nullptr);
                if (std::memcmp(&viewport, &hostViewport, sizeof(viewport)) != 0 ||
                    std::memcmp(&scissor, &hostScissor, sizeof(scissor)) != 0 ||
                    topology != D3D11_PRIMITIVE_TOPOLOGY_POINTLIST || shader != nullptr)
                {
                    m_Messages.emplace_back("DX11Render did not restore the host's pipeline state");
                }

                m_Context->OMSetRenderTargets(0, nullptr, nullptr);
                m_Context->CopyResource(staging.Get(), target.Get());
                D3D11_MAPPED_SUBRESOURCE mapped = {};
                if (FAILED(m_Context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
                {
                    m_Messages.emplace_back("Could not map the readback texture");
                    return image;
                }
                image.Pixels.resize(static_cast<size_t>(width) * height * 4);
                const bool isBgra =
                    m_Format == DXGI_FORMAT_B8G8R8A8_UNORM || m_Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                for (uint32_t row = 0; row < height; row++)
                {
                    uint8_t* out = &image.Pixels[static_cast<size_t>(row) * width * 4];
                    std::memcpy(out,
                                static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(row) * mapped.RowPitch,
                                static_cast<size_t>(width) * 4);
                    if (isBgra)
                    {
                        for (uint32_t x = 0; x < width; x++)
                            std::swap(out[x * 4], out[x * 4 + 2]);
                    }
                }
                m_Context->Unmap(staging.Get(), 0);
                return image;
            }

            size_t CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
            {
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = width;
                desc.Height = height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA data = {};
                data.pSysMem = texels.data();
                data.SysMemPitch = width * 4;
                ComPtr<ID3D11Texture2D> texture;
                ComPtr<ID3D11ShaderResourceView> view;
                m_Device->CreateTexture2D(&desc, &data, &texture);
                m_Device->CreateShaderResourceView(texture.Get(), nullptr, &view);
                m_Textures.push_back(view);
                return m_Textures.size() - 1;
            }

            TextureID GetTextureID(size_t texture) override { return DX11GetTextureID(m_Textures[texture].Get()); }

            TextureID GetRawTextureID(size_t texture) override { return MakeTextureID(m_Textures[texture].Get()); }

            std::vector<std::string> TakeMessages() override
            {
                if (m_InfoQueue != nullptr)
                {
                    const UINT64 count = m_InfoQueue->GetNumStoredMessages();
                    for (UINT64 i = 0; i < count; i++)
                    {
                        SIZE_T length = 0;
                        m_InfoQueue->GetMessage(i, nullptr, &length);
                        std::vector<uint8_t> storage(length);
                        D3D11_MESSAGE* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
                        if (SUCCEEDED(m_InfoQueue->GetMessage(i, message, &length)) &&
                            message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING)
                        {
                            m_Messages.emplace_back(message->pDescription, message->DescriptionByteLength);
                        }
                    }
                    m_InfoQueue->ClearStoredMessages();
                }
                std::vector<std::string> messages = std::move(m_Messages);
                m_Messages.clear();
                return messages;
            }

        private:
            ComPtr<ID3D11Device> m_Device;
            ComPtr<ID3D11DeviceContext> m_Context;
            ComPtr<ID3D11InfoQueue> m_InfoQueue;
            DXGI_FORMAT m_Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            std::vector<ComPtr<ID3D11ShaderResourceView>> m_Textures;
            std::vector<std::string> m_Messages;
        };
    } // namespace

    std::unique_ptr<BackendHarness> CreateDX11Harness()
    {
        return std::make_unique<DX11Harness>();
    }
} // namespace Carbon

#endif
