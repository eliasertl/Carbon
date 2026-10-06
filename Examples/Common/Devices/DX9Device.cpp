// The Direct3D 9 device of the examples: the device of the window, reset when the window's size changes, and an
// offscreen render target for screenshots. DX9Minimal shows the same with the host's own drawing next to Carbon's.

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <d3d9.h>
#include <wrl/client.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <Carbon/Backends/DX9/DX9Backend.h>

#include "GraphicsDevice.h"

namespace Example
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        class DX9Device : public GraphicsDevice
        {
        public:
            std::string_view GetName() const override { return "DX9"; }

            void SetWindowHints() const override { glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); }

            // A Direct3D 9 device belongs to a window, also when it renders offscreen.
            bool NeedsWindowOffscreen() const override { return true; }

            bool Create(GLFWwindow* window, bool isOffscreen, uint32_t width, uint32_t height) override
            {
                m_Direct3D.Attach(Direct3DCreate9(D3D_SDK_VERSION));
                if (m_Direct3D == nullptr)
                {
                    std::fprintf(stderr, "Direct3D 9 is not available\n");
                    return false;
                }
                m_Parameters.Windowed = TRUE;
                m_Parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
                m_Parameters.BackBufferFormat = D3DFMT_UNKNOWN;
                m_Parameters.BackBufferWidth = isOffscreen ? 1 : width;
                m_Parameters.BackBufferHeight = isOffscreen ? 1 : height;
                m_Parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
                m_Parameters.hDeviceWindow = glfwGetWin32Window(window);
                if (FAILED(m_Direct3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_Parameters.hDeviceWindow,
                                                    D3DCREATE_HARDWARE_VERTEXPROCESSING, &m_Parameters, &m_Device)))
                {
                    std::fprintf(stderr, "No Direct3D 9 device could be created\n");
                    return false;
                }
                m_Width = width;
                m_Height = height;
                if (isOffscreen &&
                    (FAILED(m_Device->CreateRenderTarget(width, height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                         &m_Offscreen, nullptr)) ||
                     FAILED(m_Device->SetRenderTarget(0, m_Offscreen.Get()))))
                {
                    std::fprintf(stderr, "The offscreen render target could not be created\n");
                    return false;
                }
                return true;
            }

            bool InitCarbon() override
            {
                Carbon::DX9InitInfo info;
                info.Device = m_Device.Get();
                info.ColorFormat = Carbon::TextureFormat::BGRA8Unorm;
                return Carbon::DX9Init(info);
            }

            void ShutdownCarbon() override { Carbon::DX9Shutdown(); }

            bool BeginFrame(uint32_t width, uint32_t height) override
            {
                if (m_Offscreen != nullptr)
                    return true;
                const HRESULT state = m_Device->TestCooperativeLevel();
                if (state == D3DERR_DEVICELOST)
                    return false; // another application owns the display; try again later
                // A new size, or a lost device that can be reset now: Carbon's D3DPOOL_DEFAULT objects go first.
                if (state == D3DERR_DEVICENOTRESET || width != m_Width || height != m_Height)
                {
                    Carbon::DX9InvalidateDeviceObjects();
                    m_Parameters.BackBufferWidth = width;
                    m_Parameters.BackBufferHeight = height;
                    if (FAILED(m_Device->Reset(&m_Parameters)))
                        return false;
                    m_Width = width;
                    m_Height = height;
                }
                return true;
            }

            void Render(Carbon::Color background) override
            {
                m_Device->Clear(0, nullptr, D3DCLEAR_TARGET,
                                D3DCOLOR_COLORVALUE(background.R, background.G, background.B, 1.0f), 1.0f, 0);
                m_Device->BeginScene();
                Carbon::DX9Render();
                m_Device->EndScene();
            }

            void EndFrame() override
            {
                if (m_Offscreen == nullptr)
                    m_Device->Present(nullptr, nullptr, nullptr, nullptr);
            }

            bool ReadPixels(std::vector<uint8_t>& pixels) override
            {
                ComPtr<IDirect3DSurface9> readback;
                D3DLOCKED_RECT locked = {};
                if (FAILED(m_Device->CreateOffscreenPlainSurface(m_Width, m_Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                                 &readback, nullptr)) ||
                    FAILED(m_Device->GetRenderTargetData(m_Offscreen.Get(), readback.Get())) ||
                    FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
                    return false;
                // A8R8G8B8 is B, G, R, A in memory.
                const size_t rowBytes = static_cast<size_t>(m_Width) * 4;
                pixels.resize(rowBytes * m_Height);
                for (uint32_t row = 0; row < m_Height; row++)
                {
                    uint8_t* out = &pixels[row * rowBytes];
                    std::memcpy(out,
                                static_cast<const uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
                                rowBytes);
                    for (uint32_t x = 0; x < m_Width; x++)
                        std::swap(out[x * 4], out[x * 4 + 2]);
                }
                readback->UnlockRect();
                return true;
            }

            Carbon::TextureID CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
            {
                // The managed pool survives device resets, so the texture needs no recreation.
                ComPtr<IDirect3DTexture9> texture;
                D3DLOCKED_RECT locked = {};
                if (FAILED(m_Device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture,
                                                   nullptr)) ||
                    FAILED(texture->LockRect(0, &locked, nullptr, 0)))
                    return {};
                for (uint32_t row = 0; row < height; row++)
                {
                    uint8_t* out = static_cast<uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch;
                    std::memcpy(out, texels.data() + static_cast<size_t>(row) * width * 4,
                                static_cast<size_t>(width) * 4);
                    for (uint32_t x = 0; x < width; x++)
                        std::swap(out[x * 4], out[x * 4 + 2]);
                }
                texture->UnlockRect(0);
                m_Textures.push_back(texture);
                return Carbon::MakeTextureID(texture.Get());
            }

        private:
            ComPtr<IDirect3D9> m_Direct3D;
            ComPtr<IDirect3DDevice9> m_Device;
            ComPtr<IDirect3DSurface9> m_Offscreen;
            std::vector<ComPtr<IDirect3DTexture9>> m_Textures;
            D3DPRESENT_PARAMETERS m_Parameters = {};
            uint32_t m_Width = 0;
            uint32_t m_Height = 0;
        };
    } // namespace

    std::unique_ptr<GraphicsDevice> CreateGraphicsDevice()
    {
        return std::make_unique<DX9Device>();
    }
} // namespace Example
