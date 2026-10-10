#include "ExampleApp.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "FileDrop.h"
#include "GlfwInput.h"
#include "InputMethod.h"

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

        // Replaces \uXXXX and \UXXXXXXXX escapes by the UTF-8 of their code point. Command lines on Windows reach
        // main() in the ANSI code page, so text in other scripts is passed this way.
        std::string Unescape(std::string_view text)
        {
            std::string result;
            for (size_t i = 0; i < text.size(); i++)
            {
                const size_t digitCount =
                    i + 1 < text.size() ? (text[i + 1] == 'u' ? 4 : (text[i + 1] == 'U' ? 8 : 0)) : 0;
                if (text[i] == '\\' && digitCount > 0 && i + 1 + digitCount < text.size())
                {
                    const std::string digits(text.substr(i + 2, digitCount));
                    Carbon::AppendUTF8(result, static_cast<char32_t>(std::strtoul(digits.c_str(), nullptr, 16)));
                    i += 1 + digitCount;
                    continue;
                }
                result += text[i];
            }
            return result;
        }

        // Sends what an input method would while the user converts `script` (--compose): its text without the
        // '|' that separate the clauses, the caret at the end, and the first clause as the one being converted.
        void ComposeScripted(std::string_view escaped)
        {
            const std::string script = Unescape(escaped);
            std::string text;
            std::vector<Carbon::CompositionClause> clauses;
            size_t start = 0;
            while (start <= script.size())
            {
                const size_t end = std::min(script.find('|', start), script.size());
                Carbon::CompositionClause clause;
                clause.Start = text.size();
                text.append(script, start, end - start);
                clause.End = text.size();
                clause.IsActive = clauses.empty();
                clauses.push_back(clause);
                start = end + 1;
            }
            // A single clause is plain pre-edit text that is not being converted yet.
            if (clauses.size() == 1)
                clauses.clear();
            Carbon::GetIO().AddCompositionUpdateEvent(text, text.size(), clauses);
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
        InstallInputMethod(m_Host.GetWindow());
        InstallFileDrop(m_Host.GetWindow());
        m_SystemFonts = AddSystemFallbackFonts();

        // Carbon renders through the backend of the device this executable was built for.
        m_IsBackendReady = m_Host.GetDevice().InitCarbon();

        Carbon::SetTheme(GetArguments().IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());
    }

    App::~App()
    {
        RemoveFileDrop(m_Host.GetWindow());
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
        // Files from the system, dragged over a position or dropped there.
        if (m_Host.IsScreenshotMode() && arguments.FileX >= 0.0f && m_Host.GetFrameIndex() == 8)
        {
            const std::string_view files[] = {"Pictures/Beach.jpg", "Documents/Report.pdf"};
            const float x = m_Host.GetPointerOriginX() + arguments.FileX;
            const float y = m_Host.GetPointerOriginY() + arguments.FileY;
            if (arguments.DropsFiles)
                io.AddFileDropEvent(x, y, files);
            else
                io.AddFileDragEvent(x, y, files);
        }
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
            if (frame == 10 && !arguments.Composition.empty())
                ComposeScripted(arguments.Composition);
            // A drag: pressed at the pointer, moved in two steps, and held while the screenshot is taken.
            if (arguments.DragX >= 0.0f)
            {
                const float startX = m_Host.GetPointerOriginX() + arguments.PointerX;
                const float startY = m_Host.GetPointerOriginY() + arguments.PointerY;
                const float endX = m_Host.GetPointerOriginX() + arguments.DragX;
                const float endY = m_Host.GetPointerOriginY() + arguments.DragY;
                if (frame == 8)
                    io.AddMouseButtonEvent(Carbon::MouseButton::Left, true);
                if (frame == 9)
                    io.AddMousePosEvent((startX + endX) * 0.5f, (startY + endY) * 0.5f);
                if (frame == 10)
                    io.AddMousePosEvent(endX, endY);
            }
        }

        Carbon::NewFrame();
        m_Build();
        Carbon::EndFrame();
        UpdateInputMethod(m_Host.GetWindow());

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
