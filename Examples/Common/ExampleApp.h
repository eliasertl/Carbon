#pragma once

#include <functional>

#include <Carbon/Carbon.h>

#include "ExampleHost.h"

namespace Example
{
    /// A complete host application around a Carbon interface: the window or screenshot target, the Carbon
    /// context, input forwarding and the frame loop. It does exactly what Examples/MinimalIntegration spells out
    /// step by step, so that the other examples can concentrate on their interface.
    class App
    {
    public:
        /// `width` and `height` are the window's size in points.
        App(int argc, char** argv, const char* title, int width, int height);
        ~App();

        App(const App&) = delete;
        App& operator=(const App&) = delete;

        bool IsReady() const { return m_Host.IsReady(); }
        Host& GetHost() { return m_Host; }
        const Arguments& GetArguments() const { return m_Host.GetArguments(); }

        /// Runs until the window is closed (or the screenshot is saved). `build` is called once per frame, between
        /// NewFrame and EndFrame, and builds the interface. Returns the process exit code.
        int Run(const std::function<void()>& build);

    private:
        Host m_Host;
        Carbon::Context* m_Context = nullptr;
    };
} // namespace Example
