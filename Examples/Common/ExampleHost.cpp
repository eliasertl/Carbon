#include "ExampleHost.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stb_image_write.h>

#include "Surface.h"

namespace Example
{
    namespace
    {
        // Screenshot mode renders a few frames first so that animations and first-frame layout have settled.
        constexpr int ScreenshotWarmupFrames = 8;
        constexpr float ScreenshotDeltaTime = 0.25f;

        void PrintString(const char* prefix, wgpu::StringView message)
        {
            const std::string_view view = message;
            std::fprintf(stderr, "%s%.*s\n", prefix, static_cast<int>(view.size()), view.data());
        }
    } // namespace

    Arguments ParseArguments(int argc, char** argv)
    {
        Arguments arguments;
        for (int i = 1; i < argc; i++)
        {
            const std::string_view option = argv[i];
            const bool hasValue = i + 1 < argc;
            if (option == "--screenshot" && hasValue)
            {
                arguments.ScreenshotPath = argv[++i];
            }
            else if (option == "--theme" && hasValue)
            {
                const std::string_view theme = argv[++i];
                if (theme != "light" && theme != "dark")
                    std::fprintf(stderr, "Unknown theme '%s'; expected light or dark\n", argv[i]);
                arguments.IsDark = theme == "dark";
            }
            else if (option == "--scale" && hasValue)
            {
                arguments.Scale = static_cast<float>(std::atof(argv[++i]));
                if (arguments.Scale <= 0.0f)
                {
                    std::fprintf(stderr, "Invalid scale '%s'\n", argv[i]);
                    arguments.Scale = 0.0f;
                }
            }
            else if (option == "--size" && hasValue)
            {
                const std::string size = argv[++i];
                const size_t separator = size.find('x');
                if (separator != std::string::npos)
                {
                    arguments.Width = std::atoi(size.substr(0, separator).c_str());
                    arguments.Height = std::atoi(size.substr(separator + 1).c_str());
                }
                if (arguments.Width <= 0 || arguments.Height <= 0)
                {
                    std::fprintf(stderr, "Invalid size '%s'; expected <width>x<height>\n", argv[i]);
                    arguments.Width = 0;
                    arguments.Height = 0;
                }
            }
            else
            {
                std::fprintf(stderr, "Unknown option '%s'\n", argv[i]);
                std::fprintf(stderr,
                             "Options: --screenshot <file.png>  --theme light|dark  --scale <factor>  "
                             "--size <width>x<height>\n");
            }
        }
        return arguments;
    }

    Host::Host(const Arguments& arguments, const char* title, int width, int height) : m_Arguments(arguments)
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        // Screenshot mode runs unattended: a failed assertion of the debug runtime must end up on stderr, not in
        // a dialog that waits for a click.
        if (IsScreenshotMode())
        {
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        }
#endif
        if (arguments.Width > 0 && arguments.Height > 0)
        {
            width = arguments.Width;
            height = arguments.Height;
        }

        if (IsScreenshotMode())
        {
            m_ContentScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
            m_PixelWidth = static_cast<uint32_t>(std::lround(static_cast<float>(width) * m_ContentScale));
            m_PixelHeight = static_cast<uint32_t>(std::lround(static_cast<float>(height) * m_ContentScale));
            m_DeltaTime = ScreenshotDeltaTime;
        }
        else
        {
            glfwSetErrorCallback([](int code, const char* description)
                                 { std::fprintf(stderr, "GLFW error %d: %s\n", code, description); });
            if (!glfwInit())
                return;
            m_IsGlfwInitialized = true;

            // Carbon renders through WebGPU; GLFW must not create an OpenGL context.
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            // Let the window system scale the window with the monitor unless a scale was forced.
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, arguments.Scale > 0.0f ? GLFW_FALSE : GLFW_TRUE);
            const float windowScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
            m_Window = glfwCreateWindow(static_cast<int>(std::lround(static_cast<float>(width) * windowScale)),
                                        static_cast<int>(std::lround(static_cast<float>(height) * windowScale)), title,
                                        nullptr, nullptr);
            if (m_Window == nullptr)
                return;
            glfwSetWindowUserPointer(m_Window, this);
        }

        if (!CreateDevice())
            return;

        if (IsScreenshotMode())
        {
            // RGBA so the pixels can be written to a PNG without conversion.
            m_ColorFormat = wgpu::TextureFormat::RGBA8Unorm;
            wgpu::TextureDescriptor descriptor;
            descriptor.label = "Screenshot target";
            descriptor.size = {m_PixelWidth, m_PixelHeight, 1};
            descriptor.format = m_ColorFormat;
            descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
            m_OffscreenTexture = m_Device.CreateTexture(&descriptor);
        }
        else
        {
            // Prefer a non-sRGB format: Carbon then blends in gamma space, like macOS UI.
            wgpu::SurfaceCapabilities capabilities;
            m_Surface.GetCapabilities(m_Adapter, &capabilities);
            if (capabilities.formatCount == 0)
            {
                std::fprintf(stderr, "The surface reports no supported formats\n");
                return;
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
            m_LastTime = glfwGetTime();
        }

        m_IsReady = true;
    }

    Host::~Host()
    {
        m_TargetView = nullptr;
        m_OffscreenTexture = nullptr;
        m_Surface = nullptr;
        m_Device = nullptr;
        m_Adapter = nullptr;
        m_Instance = nullptr;
        if (m_Window != nullptr)
            glfwDestroyWindow(m_Window);
        if (m_IsGlfwInitialized)
            glfwTerminate();
    }

    bool Host::CreateDevice()
    {
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

        if (m_Window != nullptr)
        {
            m_Surface = CreateSurfaceForWindow(m_Instance, m_Window);
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
        return true;
    }

    void Host::ConfigureSurface()
    {
        wgpu::SurfaceConfiguration configuration;
        configuration.device = m_Device;
        configuration.format = m_ColorFormat;
        configuration.width = m_PixelWidth;
        configuration.height = m_PixelHeight;
        configuration.presentMode = wgpu::PresentMode::Fifo;
        m_Surface.Configure(&configuration);
        m_ConfiguredWidth = m_PixelWidth;
        m_ConfiguredHeight = m_PixelHeight;
    }

    void Host::CursorToPoints(double cursorX, double cursorY, float& x, float& y) const
    {
        // GLFW reports the cursor in screen coordinates, which are pixels on Windows and X11 but may be scaled
        // units elsewhere. Going through the framebuffer size handles both.
        int windowWidth = 1;
        int windowHeight = 1;
        if (m_Window != nullptr)
            glfwGetWindowSize(m_Window, &windowWidth, &windowHeight);
        const double pixelsPerUnitX = windowWidth > 0 ? static_cast<double>(m_PixelWidth) / windowWidth : 1.0;
        const double pixelsPerUnitY = windowHeight > 0 ? static_cast<double>(m_PixelHeight) / windowHeight : 1.0;
        x = static_cast<float>(cursorX * pixelsPerUnitX / m_ContentScale);
        y = static_cast<float>(cursorY * pixelsPerUnitY / m_ContentScale);
    }

    bool Host::BeginFrame()
    {
        if (!m_IsReady)
            return false;

        if (IsScreenshotMode())
        {
            if (m_FrameIndex >= ScreenshotWarmupFrames)
                return false;
            m_TargetView = m_OffscreenTexture.CreateView();
            return true;
        }

        while (true)
        {
            glfwPollEvents();
            if (glfwWindowShouldClose(m_Window))
                return false;

            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);
            if (framebufferWidth <= 0 || framebufferHeight <= 0)
            {
                // Minimized: nothing to render into. Sleep until something happens.
                glfwWaitEvents();
                continue;
            }
            m_PixelWidth = static_cast<uint32_t>(framebufferWidth);
            m_PixelHeight = static_cast<uint32_t>(framebufferHeight);

            if (m_Arguments.Scale > 0.0f)
            {
                m_ContentScale = m_Arguments.Scale;
            }
            else
            {
                float scaleX = 1.0f;
                float scaleY = 1.0f;
                glfwGetWindowContentScale(m_Window, &scaleX, &scaleY);
                m_ContentScale = scaleX > 0.0f ? scaleX : 1.0f;
            }

            if (m_PixelWidth != m_ConfiguredWidth || m_PixelHeight != m_ConfiguredHeight)
                ConfigureSurface();

            wgpu::SurfaceTexture surfaceTexture;
            m_Surface.GetCurrentTexture(&surfaceTexture);
            if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
                surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
            {
                // The surface went stale (resize, display change): configure it again and retry.
                ConfigureSurface();
                continue;
            }
            m_TargetView = surfaceTexture.texture.CreateView();
            break;
        }

        const double now = glfwGetTime();
        m_DeltaTime = static_cast<float>(now - m_LastTime);
        m_LastTime = now;
        return true;
    }

    void Host::EndFrame()
    {
        m_TargetView = nullptr;
        m_FrameIndex++;

        if (IsScreenshotMode())
        {
            if (m_FrameIndex == ScreenshotWarmupFrames)
                m_IsReady = SaveScreenshot();
            m_Instance.ProcessEvents();
            return;
        }

        m_Surface.Present();
        m_Instance.ProcessEvents();
    }

    bool Host::SaveScreenshot()
    {
        // Rows in a buffer copy must be aligned to 256 bytes.
        const uint32_t bytesPerRow = (m_PixelWidth * 4 + 255) & ~255u;
        const uint64_t bufferSize = static_cast<uint64_t>(bytesPerRow) * m_PixelHeight;

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
        destination.layout.rowsPerImage = m_PixelHeight;
        const wgpu::Extent3D extent = {m_PixelWidth, m_PixelHeight, 1};

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
        {
            std::fprintf(stderr, "Could not read back the screenshot\n");
            return false;
        }

        const void* pixels = buffer.GetConstMappedRange(0, static_cast<size_t>(bufferSize));
        const int written = stbi_write_png(m_Arguments.ScreenshotPath.c_str(), static_cast<int>(m_PixelWidth),
                                           static_cast<int>(m_PixelHeight), 4, pixels, static_cast<int>(bytesPerRow));
        buffer.Unmap();
        if (written == 0)
        {
            std::fprintf(stderr, "Could not write '%s'\n", m_Arguments.ScreenshotPath.c_str());
            return false;
        }
        std::printf("Saved %s (%u x %u pixels, scale %.2f)\n", m_Arguments.ScreenshotPath.c_str(), m_PixelWidth,
                    m_PixelHeight, static_cast<double>(m_ContentScale));
        return true;
    }
} // namespace Example
