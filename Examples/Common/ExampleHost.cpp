#include "ExampleHost.h"

#include <cmath>
#include <cstdio>
#include <format>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__EMSCRIPTEN__)
#include <GLFW/emscripten_glfw3.h>
#endif

#include "Screenshot.h"

namespace Example
{
    Host::Host(const Arguments& arguments, const char* title, int width, int height)
        : m_Arguments(arguments), m_Device(CreateGraphicsDevice())
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

        const bool isOffscreen = IsScreenshotMode();
        if (isOffscreen)
        {
            m_ContentScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
            m_PixelWidth = static_cast<uint32_t>(std::lround(static_cast<float>(width) * m_ContentScale));
            m_PixelHeight = static_cast<uint32_t>(std::lround(static_cast<float>(height) * m_ContentScale));
            m_DeltaTime = ScreenshotDeltaTime;
        }

        // Screenshot mode opens no window, unless the API needs one for its device; that one stays hidden.
        if (!isOffscreen || m_Device->NeedsWindowOffscreen())
        {
            glfwSetErrorCallback([](int code, const char* description)
                                 { std::fprintf(stderr, "GLFW error %d: %s\n", code, description); });
            if (!glfwInit())
                return;
            m_IsGlfwInitialized = true;

            m_Device->SetWindowHints();
            glfwWindowHint(GLFW_VISIBLE, isOffscreen ? GLFW_FALSE : GLFW_TRUE);
#if defined(__EMSCRIPTEN__)
            // The canvas's framebuffer follows the device pixel ratio; a page has no window frame.
            glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
#else
            // Let the window system scale the window with the monitor unless a scale was forced.
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, arguments.Scale > 0.0f ? GLFW_FALSE : GLFW_TRUE);
            glfwWindowHint(GLFW_DECORATED, arguments.IsFrameless ? GLFW_FALSE : GLFW_TRUE);
#endif
            const float windowScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
            // The title names the backend, since every example exists once per backend.
            const std::string windowTitle = std::format("{} ({})", title, m_Device->GetName());
            m_Window = glfwCreateWindow(static_cast<int>(std::lround(static_cast<float>(width) * windowScale)),
                                        static_cast<int>(std::lround(static_cast<float>(height) * windowScale)),
                                        windowTitle.c_str(), nullptr, nullptr);
            if (m_Window == nullptr)
            {
                std::fprintf(stderr, "The window could not be created for %.*s\n",
                             static_cast<int>(m_Device->GetName().size()), m_Device->GetName().data());
                return;
            }
            m_IsVisible = !isOffscreen;
            glfwSetWindowUserPointer(m_Window, this);
        }

        if (!isOffscreen && !UpdateWindowMetrics())
        {
            m_PixelWidth = static_cast<uint32_t>(width);
            m_PixelHeight = static_cast<uint32_t>(height);
        }
        if (!m_Device->Create(m_Window, isOffscreen, m_PixelWidth, m_PixelHeight))
            return;
#if defined(__EMSCRIPTEN__)
        // In a browser the window is the page's canvas; it follows the size of the browser window.
        emscripten::glfw3::MakeCanvasResizable(m_Window, "window");
#endif
        if (m_IsGlfwInitialized)
            m_LastTime = glfwGetTime();
        m_IsReady = true;
    }

    Host::~Host()
    {
        m_Device.reset(); // before the window its surface or context belongs to
        if (m_Window != nullptr)
            glfwDestroyWindow(m_Window);
        if (m_IsGlfwInitialized)
            glfwTerminate();
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

    bool Host::UpdateWindowMetrics()
    {
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0)
            return false;
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
        return true;
    }

    bool Host::BeginFrame()
    {
        if (!m_IsReady)
            return false;

        if (IsScreenshotMode())
        {
            if (m_FrameIndex >= ScreenshotWarmupFrames)
                return false;
            return m_Device->BeginFrame(m_PixelWidth, m_PixelHeight);
        }

#if defined(__EMSCRIPTEN__)
        // The browser calls each frame; a page never waits. GLFW applies the canvas's new size in glfwPollEvents.
        // Without a canvas size or a device the frame is skipped.
        glfwPollEvents();
        if (!UpdateWindowMetrics() || !m_Device->BeginFrame(m_PixelWidth, m_PixelHeight))
            return false;
#else
        while (true)
        {
            glfwPollEvents();
            if (glfwWindowShouldClose(m_Window))
                return false;
            // Minimized: nothing to render into. Sleep until something happens. A device that cannot render now
            // (a lost Direct3D 9 device) is asked again shortly.
            if (!UpdateWindowMetrics())
            {
                glfwWaitEvents();
                continue;
            }
            if (m_Device->BeginFrame(m_PixelWidth, m_PixelHeight))
                break;
            glfwWaitEventsTimeout(0.1);
        }
#endif

        const double now = glfwGetTime();
        m_DeltaTime = static_cast<float>(now - m_LastTime);
        m_LastTime = now;
        return true;
    }

    void Host::EndFrame()
    {
        m_Device->EndFrame();
        m_FrameIndex++;
        if (IsScreenshotMode() && m_FrameIndex == ScreenshotWarmupFrames)
            m_IsReady = SaveScreenshot();
    }

    void Host::SetScreenshotArea(float x, float y, float width, float height, float anchorX, float anchorY)
    {
        m_Area[0] = x;
        m_Area[1] = y;
        m_Area[2] = width;
        m_Area[3] = height;
        m_PointerOriginX = anchorX;
        m_PointerOriginY = anchorY;
    }

    bool Host::SaveScreenshot()
    {
        std::vector<uint8_t> pixels;
        if (!m_Device->ReadPixels(pixels))
        {
            std::fprintf(stderr, "Could not read back the screenshot\n");
            return false;
        }
        const float* area = m_Area[2] > 0.0f ? m_Area : nullptr;
        return Example::SaveScreenshot(m_Arguments, pixels.data(), m_PixelWidth, m_PixelHeight, m_PixelWidth * 4,
                                       m_ContentScale, area);
    }
} // namespace Example
