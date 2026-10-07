#pragma once

#include <functional>

#include <Carbon/Carbon.h>

#include "ExampleHost.h"
#include "SystemFonts.h"

namespace Example
{
    /// A complete host application around a Carbon interface: the window or screenshot target, the Carbon
    /// context, the backend of the graphics device this executable was built for, input forwarding and the frame
    /// loop. It does what the *Minimal examples spell out step by step, so that the other examples can concentrate
    /// on their interface.
    class App
    {
    public:
        /// `width` and `height` are the window's size in points. A frameless window has no system title bar or
        /// border; the example draws its own.
        App(int argc, char** argv, const char* title, int width, int height, bool isFrameless = false);
        ~App();

        App(const App&) = delete;
        App& operator=(const App&) = delete;

        bool IsReady() const { return m_Host.IsReady() && m_IsBackendReady; }
        Host& GetHost() { return m_Host; }
        const Arguments& GetArguments() const { return m_Host.GetArguments(); }
        /// The fonts of the system that were added as fallbacks, such as an emoji font.
        const SystemFallbackFonts& GetSystemFonts() const { return m_SystemFonts; }

        /// Runs until the window is closed (or the screenshot is saved). `build` is called once per frame, between
        /// NewFrame and EndFrame, and builds the interface. Returns the process exit code. In a browser the page
        /// drives the frames and this never returns; what `build` refers to stays alive.
        int Run(const std::function<void()>& build);

    private:
        /// Builds and renders one frame. False when there is no frame to render.
        bool RunFrame();

    private:
        Host m_Host;
        Carbon::Context* m_Context = nullptr;
        bool m_IsBackendReady = false;
        SystemFallbackFonts m_SystemFonts;
        std::function<void()> m_Build;
    };
} // namespace Example
