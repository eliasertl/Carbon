#include "ExampleApp.h"

#include <cstdio>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "GlfwInput.h"

namespace Example
{
    namespace
    {
        Arguments ParseArgumentsFor(int argc, char** argv, bool isFrameless)
        {
            Arguments arguments = ParseArguments(argc, argv);
            arguments.IsFrameless = isFrameless;
            return arguments;
        }
    } // namespace

    App::App(int argc, char** argv, const char* title, int width, int height, bool isFrameless)
        : m_Host(ParseArgumentsFor(argc, argv, isFrameless), title, width, height)
    {
        if (!m_Host.IsReady())
            return;

        Carbon::ContextDescription description;
        description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message)
        {
            // Carbon never prints on its own; the host decides. The examples show warnings and errors.
            if (level < Carbon::LogLevel::Warning)
                return;
            const std::string_view name = Carbon::ToString(level);
            std::fprintf(stderr, "[%.*s] %.*s: %.*s\n", static_cast<int>(name.size()), name.data(),
                         static_cast<int>(source.size()), source.data(), static_cast<int>(message.size()),
                         message.data());
        };
        // A failed check was already logged as Fatal above. The examples carry on instead of breaking into a
        // debugger, which Carbon would do in debug builds when no handler is set.
        description.Callbacks.AssertFailed = [](const Carbon::AssertInfo&) {};
        InstallPlatformCallbacks(m_Host.GetWindow(), description.Callbacks);
        m_Context = Carbon::CreateContext(description);
        InstallInputCallbacks(m_Host.GetWindow());

        // Carbon renders through the backend of the device this executable was built for.
        m_IsBackendReady = m_Host.GetDevice().InitCarbon();

        Carbon::SetTheme(GetArguments().IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());
    }

    App::~App()
    {
        if (m_Context != nullptr)
        {
            if (m_IsBackendReady)
                m_Host.GetDevice().ShutdownCarbon();
            Carbon::DestroyContext(m_Context);
        }
    }

    bool App::RunFrame()
    {
        if (!m_Host.BeginFrame())
            return false;

        Carbon::IO& io = Carbon::GetIO();
        io.SetDisplaySize(m_Host.GetWidth(), m_Host.GetHeight());
        io.SetContentScale(m_Host.GetContentScale());
        io.SetDeltaTime(m_Host.GetDeltaTime());

        // Screenshots can be taken with the pointer somewhere, or after a click: the input is scripted. It starts
        // once the layout has settled, so that positions relative to a section (--section) are final.
        const Arguments& arguments = GetArguments();
        if (m_Host.IsScreenshotMode() && arguments.PointerX >= 0.0f)
        {
            const int frame = m_Host.GetFrameIndex();
            const Carbon::MouseButton button =
                arguments.ClickButton == 1 ? Carbon::MouseButton::Right : Carbon::MouseButton::Left;
            if (frame == 6)
                io.AddMousePosEvent(m_Host.GetPointerOriginX() + arguments.PointerX,
                                    m_Host.GetPointerOriginY() + arguments.PointerY);
            if (frame == 8 && arguments.ClickButton >= 0)
                io.AddMouseButtonEvent(button, true);
            if (frame == 9 && arguments.ClickButton >= 0)
                io.AddMouseButtonEvent(button, false);
        }

        Carbon::NewFrame();
        m_Build();
        Carbon::EndFrame();

        // The host clears to the theme's background, which glides during a theme switch, and Carbon draws on top.
        m_Host.GetDevice().Render(Carbon::GetStyleColor(Carbon::StyleColor::Background));
        m_Host.EndFrame();
        return true;
    }

    int App::Run(const std::function<void()>& build)
    {
        if (!IsReady())
            return 1;
        m_Build = build;

#if defined(__EMSCRIPTEN__)
        // The browser calls the frame function once per display refresh. Simulating an infinite loop leaves main
        // without returning, so the application and everything `build` refers to stay alive.
        emscripten_set_main_loop_arg([](void* app) { static_cast<App*>(app)->RunFrame(); }, this, 0, true);
        return 0;
#else
        while (RunFrame())
        {
        }
        return m_Host.IsReady() ? 0 : 1;
#endif
    }
} // namespace Example
