// Minimal integration of Carbon into a host application that renders with Direct3D 11.
//
// The host owns the window, the device, the swap chain and the render target. Each frame it forwards input to
// Carbon, builds the interface, draws its own content and then lets Carbon add the interface on top, into the same
// render target. Carbon saves and restores all pipeline state it touches. The Direct3D chores (device, swap chain,
// the triangle) live in the DX11Host class below; everything Carbon-specific is in main(). The other
// *Minimal examples do the same with the other backends.
//
//   DX11Minimal [--theme light|dark] [--scale <factor>] [--size <w>x<h>] [--screenshot <file.png>]

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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
#include <Carbon/Carbon.h>

#include "ExampleArguments.h"
#include "GlfwInput.h"
#include "Screenshot.h"

// The host triangle's bytecode, compiled from Triangle.hlsl at build time.
#include "TrianglePixel.h"
#include "TriangleVertex.h"

namespace
{
    using Microsoft::WRL::ComPtr;

    // ---------------------------------------------------------------------------------------------------------
    // The host's Direct3D 11. Nothing in here is specific to Carbon: an application already has all of it.
    // ---------------------------------------------------------------------------------------------------------
    class DX11Host
    {
    public:
        /// Creates the device and, with a window, its swap chain; without one (screenshot mode) an offscreen
        /// render target of `width` x `height` pixels.
        bool Create(GLFWwindow* window, int width, int height)
        {
            const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
            // Prefer the GPU; WARP, the software rasterizer, keeps the example running without one.
            for (const D3D_DRIVER_TYPE driver : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP})
            {
                if (SUCCEEDED(D3D11CreateDevice(nullptr, driver, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &m_Device,
                                                nullptr, &m_Context)))
                    break;
            }
            if (m_Device == nullptr)
            {
                std::fprintf(stderr, "No Direct3D 11 device could be created\n");
                return false;
            }

            if (window != nullptr)
            {
                // The swap chain comes from the factory that made the device's adapter.
                ComPtr<IDXGIDevice> dxgiDevice;
                ComPtr<IDXGIAdapter> adapter;
                ComPtr<IDXGIFactory> factory;
                m_Device.As(&dxgiDevice);
                dxgiDevice->GetAdapter(&adapter);
                adapter->GetParent(IID_PPV_ARGS(&factory));
                DXGI_SWAP_CHAIN_DESC desc = {};
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
            }
            else
            {
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = static_cast<UINT>(width);
                desc.Height = static_cast<UINT>(height);
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_RENDER_TARGET;
                m_Device->CreateTexture2D(&desc, nullptr, &m_Target);
                m_Device->CreateRenderTargetView(m_Target.Get(), nullptr, &m_TargetView);
                m_Width = width;
                m_Height = height;
            }

            m_Device->CreateVertexShader(g_TriangleVertexShader, sizeof(g_TriangleVertexShader), nullptr,
                                         &m_VertexShader);
            m_Device->CreatePixelShader(g_TrianglePixelShader, sizeof(g_TrianglePixelShader), nullptr, &m_PixelShader);
            D3D11_BUFFER_DESC constants = {};
            constants.ByteWidth = 16;
            constants.Usage = D3D11_USAGE_DEFAULT;
            constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            m_Device->CreateBuffer(&constants, nullptr, &m_Constants);
            return m_VertexShader != nullptr && m_PixelShader != nullptr && m_Constants != nullptr;
        }

        ID3D11Device* GetDevice() const { return m_Device.Get(); }
        ID3D11DeviceContext* GetContext() const { return m_Context.Get(); }
        bool IsOffscreen() const { return m_SwapChain == nullptr; }

        /// Size of the render target in pixels.
        void GetSize(int& width, int& height) const
        {
            width = m_Width;
            height = m_Height;
        }

        /// Resizes the swap chain's buffers to the window's framebuffer, when that changed.
        void Resize(int width, int height)
        {
            if (IsOffscreen() || (width == m_Width && height == m_Height))
                return;
            m_TargetView.Reset();
            m_Target.Reset();
            m_SwapChain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0);
            m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&m_Target));
            m_Device->CreateRenderTargetView(m_Target.Get(), nullptr, &m_TargetView);
            m_Width = width;
            m_Height = height;
        }

        /// Binds the render target and clears it to `clear`.
        void BeginFrame(Carbon::Color clear) const
        {
            const float color[4] = {clear.R, clear.G, clear.B, 1.0f};
            m_Context->ClearRenderTargetView(m_TargetView.Get(), color);
            ID3D11RenderTargetView* views[1] = {m_TargetView.Get()};
            m_Context->OMSetRenderTargets(1, views, nullptr);
        }

        /// Draws the triangle into `area`, in pixels from the top-left corner.
        void DrawTriangle(const Carbon::Rect& area, float brightness) const
        {
            if (area.IsEmpty())
                return;
            const D3D11_VIEWPORT viewport = {area.X, area.Y, area.Width, area.Height, 0.0f, 1.0f};
            m_Context->RSSetViewports(1, &viewport);
            const float constants[4] = {brightness, 0.0f, 0.0f, 0.0f};
            m_Context->UpdateSubresource(m_Constants.Get(), 0, nullptr, constants, 0, 0);
            // The vertices come from SV_VertexID, so there is no vertex buffer or input layout.
            m_Context->IASetInputLayout(nullptr);
            m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_Context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
            m_Context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
            ID3D11Buffer* buffers[1] = {m_Constants.Get()};
            m_Context->PSSetConstantBuffers(0, 1, buffers);
            m_Context->Draw(3, 0);
        }

        void EndFrame() const
        {
            if (!IsOffscreen())
                m_SwapChain->Present(1, 0);
        }

        /// Reads the offscreen render target back and writes it as PNG.
        bool SaveScreenshot(const Example::Arguments& arguments, float contentScale) const
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
            if (FAILED(m_Context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            {
                std::fprintf(stderr, "The screenshot could not be read back\n");
                return false;
            }
            const bool saved = Example::SaveScreenshot(arguments, static_cast<const uint8_t*>(mapped.pData),
                                                       static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height),
                                                       mapped.RowPitch, contentScale);
            m_Context->Unmap(staging.Get(), 0);
            return saved;
        }

    private:
        ComPtr<ID3D11Device> m_Device;
        ComPtr<ID3D11DeviceContext> m_Context;
        ComPtr<IDXGISwapChain> m_SwapChain;
        ComPtr<ID3D11Texture2D> m_Target;
        ComPtr<ID3D11RenderTargetView> m_TargetView;
        ComPtr<ID3D11VertexShader> m_VertexShader;
        ComPtr<ID3D11PixelShader> m_PixelShader;
        ComPtr<ID3D11Buffer> m_Constants;
        int m_Width = 0;
        int m_Height = 0;
    };

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
        Text("Carbon's interface and the host's triangle are drawn into the same Direct3D 11 render target.",
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
        Text("Drawn by the host in the same render target", {.Style = TextStyle::Caption1, .Secondary = true});
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

    // ---- 0. The host's window and Direct3D 11 device -----------------------------------------------------------
    // Screenshot mode opens no window and renders into an offscreen texture at a fixed scale.
    glfwSetErrorCallback([](int code, const char* description)
                         { std::fprintf(stderr, "GLFW error %d: %s\n", code, description); });
    if (!glfwInit())
        return 1;
    float contentScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
    const int pixelWidth = static_cast<int>(std::lround(width * contentScale));
    const int pixelHeight = static_cast<int>(std::lround(height * contentScale));
    GLFWwindow* window = nullptr;
    if (!isScreenshot)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, arguments.Scale <= 0.0f ? GLFW_TRUE : GLFW_FALSE);
        window =
            glfwCreateWindow(pixelWidth, pixelHeight, "Carbon - Direct3D 11 Minimal Integration", nullptr, nullptr);
        if (window == nullptr)
        {
            glfwTerminate();
            return 1;
        }
    }
    int exitCode = 0;
    {
        DX11Host host;
        if (!host.Create(window, pixelWidth, pixelHeight))
        {
            glfwTerminate();
            return 1;
        }

        // ---- 1. Create the Carbon context and install the Direct3D 11 backend --------------------------------
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

        Carbon::DX11InitInfo info;
        info.Device = host.GetDevice();
        info.Context = host.GetContext();
        info.ColorFormat = isScreenshot ? Carbon::TextureFormat::RGBA8Unorm : Carbon::TextureFormat::BGRA8Unorm;
        if (!Carbon::DX11Init(info))
            return 1;

        Settings settings;
        settings.IsDark = arguments.IsDark;
        Carbon::SetTheme(settings.IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());

        // ---- 2. Forward the window's input events to Carbon --------------------------------------------------
        // WebGPUMinimal writes these callbacks out one by one; this is the same code.
        Example::InstallInputCallbacks(window);

        // ---- 3. The frame loop -------------------------------------------------------------------------------
        double lastTime = glfwGetTime();
        for (int frameIndex = 0;; frameIndex++)
        {
            float deltaTime = Example::ScreenshotDeltaTime;
            if (!isScreenshot)
            {
                glfwPollEvents();
                if (glfwWindowShouldClose(window))
                    break;
                float scaleX = 1.0f;
                float scaleY = 1.0f;
                glfwGetWindowContentScale(window, &scaleX, &scaleY);
                contentScale = arguments.Scale > 0.0f ? arguments.Scale : scaleX;
                const double now = glfwGetTime();
                deltaTime = static_cast<float>(now - lastTime);
                lastTime = now;
                int framebufferWidth = 0;
                int framebufferHeight = 0;
                glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
                if (framebufferWidth == 0 || framebufferHeight == 0)
                {
                    glfwWaitEvents(); // minimized
                    continue;
                }
                host.Resize(framebufferWidth, framebufferHeight);
            }
            else if (frameIndex == Example::ScreenshotWarmupFrames)
            {
                break;
            }
            int targetWidth = 0;
            int targetHeight = 0;
            host.GetSize(targetWidth, targetHeight);

            // Display metrics and timing, every frame, before NewFrame.
            Carbon::IO& io = Carbon::GetIO();
            io.SetDisplaySize(static_cast<float>(targetWidth) / contentScale,
                              static_cast<float>(targetHeight) / contentScale);
            io.SetContentScale(contentScale);
            io.SetDeltaTime(deltaTime);

            Carbon::NewFrame();
            BuildInterface(settings);
            Carbon::EndFrame();

            // The render target belongs to the host. It clears it to the theme's background, draws its own
            // content, then Carbon's interface on top, into the render target that is bound.
            host.BeginFrame(Carbon::GetStyleColor(Carbon::StyleColor::Background));
            const Carbon::Rect area = settings.HostArea;
            host.DrawTriangle(
                Carbon::Rect(std::round(area.X * contentScale), std::round(area.Y * contentScale),
                             std::round(area.Width * contentScale), std::round(area.Height * contentScale)),
                settings.Brightness);
            Carbon::DX11Render();
            host.EndFrame();

            if (isScreenshot && frameIndex == Example::ScreenshotWarmupFrames - 1 &&
                !host.SaveScreenshot(arguments, contentScale))
                exitCode = 1;
        }

        // ---- 4. Shut down before the device goes away --------------------------------------------------------
        Carbon::DX11Shutdown();
        Carbon::DestroyContext(context);
    }
    if (window != nullptr)
        glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
