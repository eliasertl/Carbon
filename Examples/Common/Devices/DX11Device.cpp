// The Direct3D 11 device of the examples: the device, a flip-model swap chain for the window, or an offscreen render
// target for screenshots. DX11Minimal shows the same with the host's own drawing next to Carbon's.

#include <cstdio>
#include <cstring>
#include <vector>

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl/client.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <Carbon/Backends/DX11/DX11Backend.h>

#include "GraphicsDevice.h"

namespace Example
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        class DX11Device : public GraphicsDevice
        {
        public:
            std::string_view GetName() const override { return "DX11"; }

            void SetWindowHints() const override { glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); }

            bool Create(GLFWwindow* window, bool isOffscreen, uint32_t width, uint32_t height) override
            {
                const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
                // Prefer the GPU; WARP, the software rasterizer, keeps the examples running without one.
                for (const D3D_DRIVER_TYPE driver : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP})
                {
                    if (SUCCEEDED(D3D11CreateDevice(nullptr, driver, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                                    &m_Device, nullptr, &m_Context)))
                        break;
                }
                if (m_Device == nullptr)
                {
                    std::fprintf(stderr, "No Direct3D 11 device could be created\n");
                    return false;
                }
                m_Width = width;
                m_Height = height;

                if (isOffscreen)
                {
                    D3D11_TEXTURE2D_DESC desc = {};
                    desc.Width = width;
                    desc.Height = height;
                    desc.MipLevels = 1;
                    desc.ArraySize = 1;
                    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    desc.SampleDesc.Count = 1;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
                    m_Device->CreateTexture2D(&desc, nullptr, &m_Target);
                    m_Device->CreateRenderTargetView(m_Target.Get(), nullptr, &m_TargetView);
                    m_ColorFormat = Carbon::TextureFormat::RGBA8Unorm;
                    return m_TargetView != nullptr;
                }

                // The swap chain comes from the factory that made the device's adapter.
                ComPtr<IDXGIDevice> dxgiDevice;
                ComPtr<IDXGIAdapter> adapter;
                ComPtr<IDXGIFactory> factory;
                m_Device.As(&dxgiDevice);
                dxgiDevice->GetAdapter(&adapter);
                adapter->GetParent(IID_PPV_ARGS(&factory));
                DXGI_SWAP_CHAIN_DESC desc = {};
                desc.BufferDesc.Width = width;
                desc.BufferDesc.Height = height;
                desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                desc.BufferCount = 2;
                desc.OutputWindow = glfwGetWin32Window(window);
                desc.Windowed = TRUE;
                desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                if (FAILED(factory->CreateSwapChain(m_Device.Get(), &desc, &m_SwapChain)))
                {
                    std::fprintf(stderr, "The swap chain could not be created\n");
                    return false;
                }
                factory->MakeWindowAssociation(desc.OutputWindow, DXGI_MWA_NO_ALT_ENTER);
                m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&m_Target));
                m_Device->CreateRenderTargetView(m_Target.Get(), nullptr, &m_TargetView);
                m_ColorFormat = Carbon::TextureFormat::BGRA8Unorm;
                return m_TargetView != nullptr;
            }

            bool InitCarbon() override
            {
                Carbon::DX11InitInfo info;
                info.Device = m_Device.Get();
                info.Context = m_Context.Get();
                info.ColorFormat = m_ColorFormat;
                return Carbon::DX11Init(info);
            }

            void ShutdownCarbon() override { Carbon::DX11Shutdown(); }

            bool BeginFrame(uint32_t width, uint32_t height) override
            {
                if (m_SwapChain != nullptr && (width != m_Width || height != m_Height))
                {
                    // Every reference to the buffers goes before they are resized.
                    m_Context->OMSetRenderTargets(0, nullptr, nullptr);
                    m_TargetView.Reset();
                    m_Target.Reset();
                    m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
                    m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&m_Target));
                    m_Device->CreateRenderTargetView(m_Target.Get(), nullptr, &m_TargetView);
                    m_Width = width;
                    m_Height = height;
                }
                return m_TargetView != nullptr;
            }

            void Render(Carbon::Color background) override
            {
                const float color[4] = {background.R, background.G, background.B, 1.0f};
                m_Context->ClearRenderTargetView(m_TargetView.Get(), color);
                ID3D11RenderTargetView* views[1] = {m_TargetView.Get()};
                m_Context->OMSetRenderTargets(1, views, nullptr);
                Carbon::DX11Render();
            }

            void EndFrame() override
            {
                if (m_SwapChain != nullptr)
                    m_SwapChain->Present(1, 0);
            }

            bool ReadPixels(std::vector<uint8_t>& pixels) override
            {
                D3D11_TEXTURE2D_DESC desc = {};
                m_Target->GetDesc(&desc);
                desc.Usage = D3D11_USAGE_STAGING;
                desc.BindFlags = 0;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                ComPtr<ID3D11Texture2D> staging;
                m_Device->CreateTexture2D(&desc, nullptr, &staging);
                m_Context->CopyResource(staging.Get(), m_Target.Get());
                D3D11_MAPPED_SUBRESOURCE mapped = {};
                if (staging == nullptr || FAILED(m_Context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
                    return false;
                const size_t rowBytes = static_cast<size_t>(m_Width) * 4;
                pixels.resize(rowBytes * m_Height);
                for (uint32_t row = 0; row < m_Height; row++)
                    std::memcpy(&pixels[row * rowBytes],
                                static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(row) * mapped.RowPitch,
                                rowBytes);
                m_Context->Unmap(staging.Get(), 0);
                return true;
            }

            Carbon::TextureID CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
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
                return Carbon::MakeTextureID(view.Get());
            }

        private:
            ComPtr<ID3D11Device> m_Device;
            ComPtr<ID3D11DeviceContext> m_Context;
            ComPtr<IDXGISwapChain> m_SwapChain;
            ComPtr<ID3D11Texture2D> m_Target;
            ComPtr<ID3D11RenderTargetView> m_TargetView;
            std::vector<ComPtr<ID3D11ShaderResourceView>> m_Textures;
            Carbon::TextureFormat m_ColorFormat = Carbon::TextureFormat::BGRA8Unorm;
            uint32_t m_Width = 0;
            uint32_t m_Height = 0;
        };
    } // namespace

    std::unique_ptr<GraphicsDevice> CreateGraphicsDevice()
    {
        return std::make_unique<DX11Device>();
    }
} // namespace Example
