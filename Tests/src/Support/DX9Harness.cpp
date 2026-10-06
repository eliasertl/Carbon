#include "Support/BackendHarness.h"

#if defined(CARBON_HAS_BACKEND_DX9)

#include <algorithm>
#include <cmath>
#include <cstring>

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <d3d9.h>
#include <windows.h>
#include <wrl/client.h>

#include "Carbon/Backends/DX9/DX9Backend.h"

namespace Carbon
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        /// Encodes a linear channel as sRGB, in 8 bits.
        DWORD EncodeSrgb(float linear)
        {
            const float clamped = std::clamp(linear, 0.0f, 1.0f);
            const float encoded =
                clamped <= 0.0031308f ? clamped * 12.92f : 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
            return static_cast<DWORD>(std::lround(encoded * 255.0f));
        }

        /// Drives the Direct3D 9 backend on a device of a hidden window. Direct3D 9 surfaces have no sRGB formats:
        /// every target is A8R8G8B8, and sRGB-ness is D3DRS_SRGBWRITEENABLE, which Carbon sets from its init info.
        /// Any pipeline state the backend fails to restore is collected as a message; Direct3D 9 has no debug
        /// layer on current Windows.
        class DX9Harness : public BackendHarness
        {
        public:
            explicit DX9Harness(bool invalidateEveryFrame) : m_InvalidateEveryFrame(invalidateEveryFrame) {}

            ~DX9Harness() override
            {
                m_Textures.clear();
                m_Device.Reset();
                m_Direct3D.Reset();
                if (m_Window != nullptr)
                    DestroyWindow(m_Window);
            }

            std::string_view GetName() const override { return m_InvalidateEveryFrame ? "DX9Invalidate" : "DX9"; }

            std::string CreateDevice() override
            {
                m_Window = CreateWindowExW(0, L"STATIC", L"Carbon DX9 tests", WS_POPUP, 0, 0, 64, 64, nullptr, nullptr,
                                           GetModuleHandleW(nullptr), nullptr);
                if (m_Window == nullptr)
                    return "No window for a Direct3D 9 device could be created";
                m_Direct3D.Attach(Direct3DCreate9(D3D_SDK_VERSION));
                if (m_Direct3D == nullptr)
                    return "Direct3D 9 is not available";
                D3DPRESENT_PARAMETERS parameters = {};
                parameters.Windowed = TRUE;
                parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
                parameters.BackBufferFormat = D3DFMT_UNKNOWN;
                parameters.BackBufferWidth = 64;
                parameters.BackBufferHeight = 64;
                parameters.hDeviceWindow = m_Window;
                for (const DWORD processing : {static_cast<DWORD>(D3DCREATE_HARDWARE_VERTEXPROCESSING),
                                               static_cast<DWORD>(D3DCREATE_SOFTWARE_VERTEXPROCESSING)})
                {
                    if (SUCCEEDED(m_Direct3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_Window,
                                                           processing | D3DCREATE_FPU_PRESERVE, &parameters,
                                                           &m_Device)))
                        break;
                }
                if (m_Device == nullptr)
                    return "No Direct3D 9 device could be created";
                D3DCAPS9 caps = {};
                m_Device->GetDeviceCaps(&caps);
                if (caps.PixelShaderVersion < D3DPS_VERSION(3, 0))
                    return "The Direct3D 9 device lacks shader model 3.0";
                return {};
            }

            bool InitBackend(TextureFormat colorFormat) override
            {
                m_IsSrgb = IsSrgbFormat(colorFormat);
                DX9InitInfo info;
                info.Device = m_Device.Get();
                info.ColorFormat = colorFormat;
                return DX9Init(info);
            }

            void ShutdownBackend() override { DX9Shutdown(); }

            RenderedImage RenderFrame(uint32_t width, uint32_t height, Color background) override
            {
                RenderedImage image;
                image.Width = width;
                image.Height = height;

                ComPtr<IDirect3DSurface9> target;
                ComPtr<IDirect3DSurface9> readback;
                ComPtr<IDirect3DSurface9> backBuffer;
                if (FAILED(m_Device->CreateRenderTarget(width, height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                        &target, nullptr)) ||
                    FAILED(m_Device->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                                 &readback, nullptr)))
                {
                    m_Messages.emplace_back("Could not create the render target");
                    return image;
                }
                m_Device->GetRenderTarget(0, &backBuffer);
                m_Device->SetRenderTarget(0, target.Get());

                // The clear color is linear like the other APIs' clear values; an sRGB target stores it encoded.
                m_Device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
                const auto toByte = [this](float channel)
                { return m_IsSrgb ? EncodeSrgb(channel) : static_cast<DWORD>(std::lround(channel * 255.0f)); };
                m_Device->Clear(0, nullptr, D3DCLEAR_TARGET,
                                D3DCOLOR_ARGB(static_cast<DWORD>(std::lround(background.A * 255.0f)),
                                              toByte(background.R), toByte(background.G), toByte(background.B)),
                                1.0f, 0);

                // State a host might have left: DX9Render must restore it.
                const D3DVIEWPORT9 hostViewport = {5, 6, 7, 8, 0.0f, 1.0f};
                m_Device->SetViewport(&hostViewport);
                const RECT hostScissor = {1, 2, 3, 4};
                m_Device->SetScissorRect(&hostScissor);
                m_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
                m_Device->SetPixelShader(nullptr);
                m_Device->BeginScene();
                if (m_InvalidateEveryFrame)
                    DX9InvalidateDeviceObjects();
                {
                    const RenderTimer timer(*this);
                    DX9Render();
                }
                m_Device->EndScene();
                D3DVIEWPORT9 viewport = {};
                m_Device->GetViewport(&viewport);
                RECT scissor = {};
                m_Device->GetScissorRect(&scissor);
                DWORD cullMode = 0;
                m_Device->GetRenderState(D3DRS_CULLMODE, &cullMode);
                DWORD isSrgbWrite = TRUE;
                m_Device->GetRenderState(D3DRS_SRGBWRITEENABLE, &isSrgbWrite);
                ComPtr<IDirect3DPixelShader9> shader;
                m_Device->GetPixelShader(&shader);
                if (std::memcmp(&viewport, &hostViewport, sizeof(viewport)) != 0 ||
                    std::memcmp(&scissor, &hostScissor, sizeof(scissor)) != 0 || cullMode != D3DCULL_CW ||
                    isSrgbWrite != FALSE || shader != nullptr)
                {
                    m_Messages.emplace_back("DX9Render did not restore the host's pipeline state");
                }

                m_Device->SetRenderTarget(0, backBuffer.Get());
                D3DLOCKED_RECT locked = {};
                if (FAILED(m_Device->GetRenderTargetData(target.Get(), readback.Get())) ||
                    FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
                {
                    m_Messages.emplace_back("Could not read the render target back");
                    return image;
                }
                // A8R8G8B8 is B, G, R, A in memory.
                image.Pixels.resize(static_cast<size_t>(width) * height * 4);
                for (uint32_t row = 0; row < height; row++)
                {
                    uint8_t* out = &image.Pixels[static_cast<size_t>(row) * width * 4];
                    std::memcpy(out,
                                static_cast<const uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
                                static_cast<size_t>(width) * 4);
                    for (uint32_t x = 0; x < width; x++)
                        std::swap(out[x * 4], out[x * 4 + 2]);
                }
                readback->UnlockRect();
                return image;
            }

            size_t CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
            {
                ComPtr<IDirect3DTexture9> texture;
                m_Device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture,
                                        nullptr);
                D3DLOCKED_RECT locked = {};
                if (texture != nullptr && SUCCEEDED(texture->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD)))
                {
                    for (uint32_t row = 0; row < height; row++)
                    {
                        uint8_t* out = static_cast<uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch;
                        std::memcpy(out, texels.data() + static_cast<size_t>(row) * width * 4,
                                    static_cast<size_t>(width) * 4);
                        for (uint32_t x = 0; x < width; x++)
                            std::swap(out[x * 4], out[x * 4 + 2]);
                    }
                    texture->UnlockRect(0);
                }
                m_Textures.push_back(texture);
                return m_Textures.size() - 1;
            }

            TextureID GetTextureID(size_t texture) override { return DX9GetTextureID(m_Textures[texture].Get()); }

            TextureID GetRawTextureID(size_t texture) override { return MakeTextureID(m_Textures[texture].Get()); }

            std::vector<std::string> TakeMessages() override
            {
                std::vector<std::string> messages = std::move(m_Messages);
                m_Messages.clear();
                return messages;
            }

        private:
            bool m_InvalidateEveryFrame = false;
            bool m_IsSrgb = false;
            HWND m_Window = nullptr;
            ComPtr<IDirect3D9> m_Direct3D;
            ComPtr<IDirect3DDevice9> m_Device;
            std::vector<ComPtr<IDirect3DTexture9>> m_Textures;
            std::vector<std::string> m_Messages;
        };
    } // namespace

    std::unique_ptr<BackendHarness> CreateDX9Harness(bool invalidateEveryFrame)
    {
        return std::make_unique<DX9Harness>(invalidateEveryFrame);
    }
} // namespace Carbon

#endif
