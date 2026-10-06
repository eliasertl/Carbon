// Minimal integration of Carbon into a host application that renders with Direct3D 9.
//
// The host owns the window, the device and the render target. Each frame it forwards input to Carbon, builds the
// interface, draws its own content and then lets Carbon add the interface on top, between the host's BeginScene and
// EndScene. Carbon captures the device state and restores it. When the window is resized the host resets the
// device, and calls DX9InvalidateDeviceObjects first, because Carbon's buffers live in D3DPOOL_DEFAULT. The Direct3D
// chores (device, reset, the triangle) live in the DX9Host class below; everything Carbon-specific is in main(). The
// other *Minimal examples do the same with the other backends.
//
//   DX9Minimal [--theme light|dark] [--scale <factor>] [--size <w>x<h>] [--screenshot <file.png>]

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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
#include <Carbon/Carbon.h>

#include "ExampleArguments.h"
#include "GlfwInput.h"
#include "Screenshot.h"

namespace
{
    using Microsoft::WRL::ComPtr;

    // ---------------------------------------------------------------------------------------------------------
    // The host's Direct3D 9. Nothing in here is specific to Carbon: an application already has all of it.
    // ---------------------------------------------------------------------------------------------------------
    class DX9Host
    {
    public:
        /// Creates the device for `window`. In screenshot mode (`isOffscreen`) it draws into an offscreen render
        /// target of `width` x `height` pixels instead of the window's back buffer.
        bool Create(GLFWwindow* window, bool isOffscreen, int width, int height)
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
            m_Parameters.BackBufferWidth = static_cast<UINT>(width);
            m_Parameters.BackBufferHeight = static_cast<UINT>(height);
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
            if (isOffscreen && (FAILED(m_Device->CreateRenderTarget(static_cast<UINT>(width), static_cast<UINT>(height),
                                                                    D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                                    &m_Offscreen, nullptr)) ||
                                FAILED(m_Device->SetRenderTarget(0, m_Offscreen.Get()))))
            {
                std::fprintf(stderr, "The offscreen render target could not be created\n");
                return false;
            }
            return true;
        }

        IDirect3DDevice9* GetDevice() const { return m_Device.Get(); }
        bool IsOffscreen() const { return m_Offscreen != nullptr; }

        /// Size of the render target in pixels.
        void GetSize(int& width, int& height) const
        {
            width = m_Width;
            height = m_Height;
        }

        /// True when the device must be reset: the window's framebuffer has a new size, or the device was lost and
        /// can be reset now.
        bool NeedsReset(int width, int height) const
        {
            if (IsOffscreen())
                return false;
            return width != m_Width || height != m_Height || m_Device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET;
        }

        /// True while the device is lost and cannot be reset yet (another application went fullscreen).
        bool IsLost() const { return m_Device->TestCooperativeLevel() == D3DERR_DEVICELOST; }

        /// Resets the device for a back buffer of `width` x `height` pixels. Everything in D3DPOOL_DEFAULT must
        /// have been released.
        void Reset(int width, int height)
        {
            m_Parameters.BackBufferWidth = static_cast<UINT>(width);
            m_Parameters.BackBufferHeight = static_cast<UINT>(height);
            if (SUCCEEDED(m_Device->Reset(&m_Parameters)))
            {
                m_Width = width;
                m_Height = height;
            }
        }

        /// Clears the render target to `clear` and begins the scene.
        void BeginFrame(Carbon::Color clear) const
        {
            m_Device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(clear.R, clear.G, clear.B, 1.0f), 1.0f, 0);
            m_Device->BeginScene();
        }

        /// Draws the triangle into `area`, in pixels from the top-left corner, with the fixed-function pipeline.
        void DrawTriangle(const Carbon::Rect& area, float brightness) const
        {
            if (area.IsEmpty())
                return;
            struct Vertex
            {
                float X, Y, Z, Rhw;
                D3DCOLOR Color;
            };
            const auto color = [brightness](float r, float g, float b)
            { return D3DCOLOR_COLORVALUE(r * brightness, g * brightness, b * brightness, 1.0f); };
            const Vertex vertices[3] = {
                {area.X + area.Width * 0.5f, area.Y + area.Height * 0.19f, 0.0f, 1.0f, color(1.0f, 0.27f, 0.23f)},
                {area.X + area.Width * 0.84f, area.Y + area.Height * 0.79f, 0.0f, 1.0f, color(0.04f, 0.52f, 1.0f)},
                {area.X + area.Width * 0.16f, area.Y + area.Height * 0.79f, 0.0f, 1.0f, color(0.19f, 0.82f, 0.35f)}};
            m_Device->SetVertexShader(nullptr);
            m_Device->SetPixelShader(nullptr);
            m_Device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
            m_Device->SetTexture(0, nullptr);
            m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
            m_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            m_Device->SetRenderState(D3DRS_LIGHTING, FALSE);
            m_Device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, vertices, sizeof(Vertex));
        }

        void EndFrame() const
        {
            m_Device->EndScene();
            if (!IsOffscreen())
                m_Device->Present(nullptr, nullptr, nullptr, nullptr);
        }

        /// Reads the offscreen render target back and writes it as PNG.
        bool SaveScreenshot(const Example::Arguments& arguments, float contentScale) const
        {
            ComPtr<IDirect3DSurface9> readback;
            D3DLOCKED_RECT locked = {};
            if (FAILED(m_Device->CreateOffscreenPlainSurface(static_cast<UINT>(m_Width), static_cast<UINT>(m_Height),
                                                             D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &readback, nullptr)) ||
                FAILED(m_Device->GetRenderTargetData(m_Offscreen.Get(), readback.Get())) ||
                FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            {
                std::fprintf(stderr, "The screenshot could not be read back\n");
                return false;
            }
            // A8R8G8B8 is B, G, R, A in memory; PNG wants R, G, B, A.
            const size_t rowBytes = static_cast<size_t>(m_Width) * 4;
            std::vector<uint8_t> pixels(rowBytes * static_cast<size_t>(m_Height));
            for (int row = 0; row < m_Height; row++)
            {
                uint8_t* out = &pixels[static_cast<size_t>(row) * rowBytes];
                std::memcpy(out, static_cast<const uint8_t*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
                            rowBytes);
                for (int x = 0; x < m_Width; x++)
                    std::swap(out[x * 4], out[x * 4 + 2]);
            }
            readback->UnlockRect();
            return Example::SaveScreenshot(arguments, pixels.data(), static_cast<uint32_t>(m_Width),
                                           static_cast<uint32_t>(m_Height), static_cast<uint32_t>(rowBytes),
                                           contentScale);
        }

    private:
        ComPtr<IDirect3D9> m_Direct3D;
        ComPtr<IDirect3DDevice9> m_Device;
        ComPtr<IDirect3DSurface9> m_Offscreen;
        D3DPRESENT_PARAMETERS m_Parameters = {};
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
        Text("Carbon's interface and the host's triangle are drawn into the same Direct3D 9 render target.",
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

    // ---- 0. The host's window and Direct3D 9 device ------------------------------------------------------------
    // A Direct3D 9 device needs a window, also in screenshot mode, where it stays hidden and the frame is rendered
    // into an offscreen render target at a fixed scale.
    glfwSetErrorCallback([](int code, const char* description)
                         { std::fprintf(stderr, "GLFW error %d: %s\n", code, description); });
    if (!glfwInit())
        return 1;
    float contentScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
    const int pixelWidth = static_cast<int>(std::lround(width * contentScale));
    const int pixelHeight = static_cast<int>(std::lround(height * contentScale));
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, isScreenshot ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, arguments.Scale <= 0.0f ? GLFW_TRUE : GLFW_FALSE);
    GLFWwindow* hostWindow =
        glfwCreateWindow(pixelWidth, pixelHeight, "Carbon - Direct3D 9 Minimal Integration", nullptr, nullptr);
    if (hostWindow == nullptr)
    {
        glfwTerminate();
        return 1;
    }
    int framebufferWidth = pixelWidth;
    int framebufferHeight = pixelHeight;
    if (!isScreenshot)
        glfwGetFramebufferSize(hostWindow, &framebufferWidth, &framebufferHeight);
    GLFWwindow* window = isScreenshot ? nullptr : hostWindow;
    int exitCode = 0;
    {
        DX9Host host;
        if (!host.Create(hostWindow, isScreenshot, framebufferWidth, framebufferHeight))
        {
            glfwDestroyWindow(hostWindow);
            glfwTerminate();
            return 1;
        }

        // ---- 1. Create the Carbon context and install the Direct3D 9 backend ---------------------------------
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

        Carbon::DX9InitInfo info;
        info.Device = host.GetDevice();
        info.ColorFormat = Carbon::TextureFormat::BGRA8Unorm; // the back buffer is not sRGB
        if (!Carbon::DX9Init(info))
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
                glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
                if (framebufferWidth == 0 || framebufferHeight == 0 || host.IsLost())
                {
                    glfwWaitEventsTimeout(0.1); // minimized, or the device is lost
                    continue;
                }
                // Carbon's objects in D3DPOOL_DEFAULT go before the reset; it creates them again when it renders.
                if (host.NeedsReset(framebufferWidth, framebufferHeight))
                {
                    Carbon::DX9InvalidateDeviceObjects();
                    host.Reset(framebufferWidth, framebufferHeight);
                }
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
            // content, then Carbon's interface on top, inside the same scene.
            host.BeginFrame(Carbon::GetStyleColor(Carbon::StyleColor::Background));
            const Carbon::Rect area = settings.HostArea;
            host.DrawTriangle(
                Carbon::Rect(std::round(area.X * contentScale), std::round(area.Y * contentScale),
                             std::round(area.Width * contentScale), std::round(area.Height * contentScale)),
                settings.Brightness);
            Carbon::DX9Render();
            host.EndFrame();

            if (isScreenshot && frameIndex == Example::ScreenshotWarmupFrames - 1 &&
                !host.SaveScreenshot(arguments, contentScale))
                exitCode = 1;
        }

        // ---- 4. Shut down before the device goes away --------------------------------------------------------
        Carbon::DX9Shutdown();
        Carbon::DestroyContext(context);
    }
    glfwDestroyWindow(hostWindow);
    glfwTerminate();
    return exitCode;
}
