// Minimal integration of Carbon into a host application that renders with OpenGL ES 3.0, natively or in a web
// browser through Emscripten (WebGL 2).
//
// The host owns the window, the OpenGL ES context and the framebuffer. Each frame it forwards input to Carbon, builds
// the interface, draws its own content and then lets Carbon add the interface on top, into the same framebuffer.
// Carbon saves and restores all OpenGL ES state it touches. The context chores (window, functions, the triangle)
// live in the OpenGLESHost class below; everything Carbon-specific is in Run and RunFrame. WebGPUMinimal,
// VulkanMinimal and OpenGLMinimal do the same with the other backends.
//
// Carbon needs no OpenGL ES loader of the host's: it takes glfwGetProcAddress and resolves what it uses itself. This
// host loads its own few functions the same way; an Android application would use eglGetProcAddress.
//
// In a browser, the page drives the frame loop (emscripten_set_main_loop), and the canvas fills the browser window,
// follows its size and the device pixel ratio. Natively:
//
//   OpenGLESMinimal [--theme light|dark] [--scale <factor>] [--size <w>x<h>] [--screenshot <file.png>]

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__EMSCRIPTEN__)
#include <GLFW/emscripten_glfw3.h>
#endif

#include <Carbon/Backends/OpenGLES/OpenGLESBackend.h>
#include <Carbon/Carbon.h>

#include "ExampleArguments.h"
#include "GlfwInput.h"
#include "Screenshot.h"

namespace
{
    // ---------------------------------------------------------------------------------------------------------
    // The host's OpenGL ES. Nothing in here is specific to Carbon: an application already has all of it.
    // ---------------------------------------------------------------------------------------------------------
#if defined(_WIN32) && !defined(_WIN64)
#define EXAMPLE_GL_CALL __stdcall
#else
#define EXAMPLE_GL_CALL
#endif

    using GLenum = unsigned int;
    using GLuint = unsigned int;
    using GLint = int;
    using GLsizei = int;
    using GLfloat = float;
    using GLchar = char;

    constexpr GLenum GlTriangles = 0x0004;
    constexpr GLenum GlUnsignedByte = 0x1401;
    constexpr GLenum GlColorBufferBit = 0x4000;
    constexpr GLenum GlVertexShader = 0x8B31;
    constexpr GLenum GlFragmentShader = 0x8B30;
    constexpr GLenum GlTexture2D = 0x0DE1;
    constexpr GLenum GlRgba = 0x1908;
    constexpr GLenum GlRgba8 = 0x8058;
    constexpr GLenum GlFramebuffer = 0x8D40;
    constexpr GLenum GlColorAttachment0 = 0x8CE0;
    constexpr GLenum GlPackAlignment = 0x0D05;

    /// The OpenGL ES functions this host uses, resolved through GLFW.
    struct HostFunctions
    {
        void(EXAMPLE_GL_CALL* Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
        void(EXAMPLE_GL_CALL* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
        void(EXAMPLE_GL_CALL* Clear)(GLenum) = nullptr;
        GLuint(EXAMPLE_GL_CALL* CreateShader)(GLenum) = nullptr;
        void(EXAMPLE_GL_CALL* ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
        void(EXAMPLE_GL_CALL* CompileShader)(GLuint) = nullptr;
        void(EXAMPLE_GL_CALL* DeleteShader)(GLuint) = nullptr;
        GLuint(EXAMPLE_GL_CALL* CreateProgram)() = nullptr;
        void(EXAMPLE_GL_CALL* AttachShader)(GLuint, GLuint) = nullptr;
        void(EXAMPLE_GL_CALL* LinkProgram)(GLuint) = nullptr;
        void(EXAMPLE_GL_CALL* DeleteProgram)(GLuint) = nullptr;
        void(EXAMPLE_GL_CALL* UseProgram)(GLuint) = nullptr;
        GLint(EXAMPLE_GL_CALL* GetUniformLocation)(GLuint, const GLchar*) = nullptr;
        void(EXAMPLE_GL_CALL* Uniform1f)(GLint, GLfloat) = nullptr;
        void(EXAMPLE_GL_CALL* GenVertexArrays)(GLsizei, GLuint*) = nullptr;
        void(EXAMPLE_GL_CALL* DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
        void(EXAMPLE_GL_CALL* BindVertexArray)(GLuint) = nullptr;
        void(EXAMPLE_GL_CALL* DrawArrays)(GLenum, GLint, GLsizei) = nullptr;
        void(EXAMPLE_GL_CALL* GenTextures)(GLsizei, GLuint*) = nullptr;
        void(EXAMPLE_GL_CALL* DeleteTextures)(GLsizei, const GLuint*) = nullptr;
        void(EXAMPLE_GL_CALL* BindTexture)(GLenum, GLuint) = nullptr;
        void(EXAMPLE_GL_CALL* TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                                          const void*) = nullptr;
        void(EXAMPLE_GL_CALL* GenFramebuffers)(GLsizei, GLuint*) = nullptr;
        void(EXAMPLE_GL_CALL* DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
        void(EXAMPLE_GL_CALL* BindFramebuffer)(GLenum, GLuint) = nullptr;
        void(EXAMPLE_GL_CALL* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
        void(EXAMPLE_GL_CALL* PixelStorei)(GLenum, GLint) = nullptr;
        void(EXAMPLE_GL_CALL* ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;

        template <typename Function>
        static bool Load(Function& function, const char* name)
        {
            function = reinterpret_cast<Function>(glfwGetProcAddress(name));
            return function != nullptr;
        }

        bool LoadAll()
        {
            return Load(Viewport, "glViewport") && Load(ClearColor, "glClearColor") && Load(Clear, "glClear") &&
                   Load(CreateShader, "glCreateShader") && Load(ShaderSource, "glShaderSource") &&
                   Load(CompileShader, "glCompileShader") && Load(DeleteShader, "glDeleteShader") &&
                   Load(CreateProgram, "glCreateProgram") && Load(AttachShader, "glAttachShader") &&
                   Load(LinkProgram, "glLinkProgram") && Load(DeleteProgram, "glDeleteProgram") &&
                   Load(UseProgram, "glUseProgram") && Load(GetUniformLocation, "glGetUniformLocation") &&
                   Load(Uniform1f, "glUniform1f") && Load(GenVertexArrays, "glGenVertexArrays") &&
                   Load(DeleteVertexArrays, "glDeleteVertexArrays") && Load(BindVertexArray, "glBindVertexArray") &&
                   Load(DrawArrays, "glDrawArrays") && Load(GenTextures, "glGenTextures") &&
                   Load(DeleteTextures, "glDeleteTextures") && Load(BindTexture, "glBindTexture") &&
                   Load(TexImage2D, "glTexImage2D") && Load(GenFramebuffers, "glGenFramebuffers") &&
                   Load(DeleteFramebuffers, "glDeleteFramebuffers") && Load(BindFramebuffer, "glBindFramebuffer") &&
                   Load(FramebufferTexture2D, "glFramebufferTexture2D") && Load(PixelStorei, "glPixelStorei") &&
                   Load(ReadPixels, "glReadPixels");
        }
    };

    class OpenGLESHost
    {
    public:
        /// Creates the window (hidden in screenshot mode, which renders into an offscreen framebuffer of `width` x
        /// `height` pixels) and its OpenGL ES 3.0 context. In a browser the window is the page's canvas and the
        /// context is WebGL 2.
        bool Create(bool isScreenshot, int width, int height, bool scaleToMonitor)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
            glfwWindowHint(GLFW_VISIBLE, isScreenshot ? GLFW_FALSE : GLFW_TRUE);
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, scaleToMonitor ? GLFW_TRUE : GLFW_FALSE);
            m_Window = glfwCreateWindow(width, height, "Carbon - OpenGL ES Minimal Integration", nullptr, nullptr);
            if (m_Window == nullptr)
            {
                std::fprintf(stderr, "No OpenGL ES 3.0 context could be created\n");
                return false;
            }
            glfwMakeContextCurrent(m_Window);
            glfwSwapInterval(1);
#if defined(__EMSCRIPTEN__)
            // The canvas fills the browser window and follows it when it is resized; the frame loop reads the new
            // framebuffer size every frame, which is all Carbon needs.
            emscripten::glfw3::MakeCanvasResizable(m_Window, "window");
#endif
            if (!m_GL.LoadAll())
            {
                std::fprintf(stderr, "An OpenGL ES function could not be loaded\n");
                return false;
            }
            if (isScreenshot)
            {
                m_GL.GenTextures(1, &m_OffscreenTexture);
                m_GL.BindTexture(GlTexture2D, m_OffscreenTexture);
                m_GL.TexImage2D(GlTexture2D, 0, static_cast<GLint>(GlRgba8), width, height, 0, GlRgba, GlUnsignedByte,
                                nullptr);
                m_GL.BindTexture(GlTexture2D, 0);
                m_GL.GenFramebuffers(1, &m_OffscreenFramebuffer);
                m_GL.BindFramebuffer(GlFramebuffer, m_OffscreenFramebuffer);
                m_GL.FramebufferTexture2D(GlFramebuffer, GlColorAttachment0, GlTexture2D, m_OffscreenTexture, 0);
                m_OffscreenWidth = width;
                m_OffscreenHeight = height;
            }
            return CreateTriangle();
        }

        ~OpenGLESHost()
        {
            if (m_Window == nullptr)
                return;
            glfwMakeContextCurrent(m_Window);
            if (m_Program != 0)
            {
                m_GL.DeleteProgram(m_Program);
                m_GL.DeleteVertexArrays(1, &m_VertexArray);
            }
            if (m_OffscreenFramebuffer != 0)
            {
                m_GL.DeleteFramebuffers(1, &m_OffscreenFramebuffer);
                m_GL.DeleteTextures(1, &m_OffscreenTexture);
            }
            glfwDestroyWindow(m_Window);
        }

        GLFWwindow* GetWindow() const { return m_Window; }
        bool IsOffscreen() const { return m_OffscreenFramebuffer != 0; }

        /// Size of the framebuffer in pixels.
        void GetSize(int& width, int& height) const
        {
            if (IsOffscreen())
            {
                width = m_OffscreenWidth;
                height = m_OffscreenHeight;
                return;
            }
            glfwGetFramebufferSize(m_Window, &width, &height);
        }

        /// Binds the framebuffer and clears it to `clear`.
        void BeginFrame(Carbon::Color clear) const
        {
            int width = 0;
            int height = 0;
            GetSize(width, height);
            m_GL.BindFramebuffer(GlFramebuffer, m_OffscreenFramebuffer);
            m_GL.Viewport(0, 0, width, height);
            m_GL.ClearColor(clear.R, clear.G, clear.B, 1.0f);
            m_GL.Clear(GlColorBufferBit);
        }

        /// Draws the triangle into `area`, in pixels from the top-left corner.
        void DrawTriangle(const Carbon::Rect& area, float brightness) const
        {
            if (area.IsEmpty())
                return;
            int width = 0;
            int height = 0;
            GetSize(width, height);
            // OpenGL ES's viewport is measured from the bottom-left corner.
            m_GL.Viewport(static_cast<GLint>(area.X), static_cast<GLint>(static_cast<float>(height) - area.GetBottom()),
                          static_cast<GLsizei>(area.Width), static_cast<GLsizei>(area.Height));
            m_GL.UseProgram(m_Program);
            m_GL.Uniform1f(m_BrightnessLocation, brightness);
            m_GL.BindVertexArray(m_VertexArray);
            m_GL.DrawArrays(GlTriangles, 0, 3);
            m_GL.BindVertexArray(0);
            m_GL.UseProgram(0);
        }

        void EndFrame() const
        {
            if (!IsOffscreen())
                glfwSwapBuffers(m_Window);
        }

        /// Reads the offscreen framebuffer back and writes it as PNG.
        bool SaveScreenshot(const Example::Arguments& arguments, float contentScale) const
        {
            const size_t rowBytes = static_cast<size_t>(m_OffscreenWidth) * 4;
            std::vector<uint8_t> pixels(rowBytes * static_cast<size_t>(m_OffscreenHeight));
            m_GL.PixelStorei(GlPackAlignment, 1);
            m_GL.ReadPixels(0, 0, m_OffscreenWidth, m_OffscreenHeight, GlRgba, GlUnsignedByte, pixels.data());
            // OpenGL ES's rows start at the bottom; PNG's at the top.
            std::vector<uint8_t> flipped(pixels.size());
            for (int row = 0; row < m_OffscreenHeight; row++)
                std::memcpy(&flipped[static_cast<size_t>(row) * rowBytes],
                            &pixels[static_cast<size_t>(m_OffscreenHeight - 1 - row) * rowBytes], rowBytes);
            return Example::SaveScreenshot(arguments, flipped.data(), static_cast<uint32_t>(m_OffscreenWidth),
                                           static_cast<uint32_t>(m_OffscreenHeight), static_cast<uint32_t>(rowBytes),
                                           contentScale);
        }

    private:
        bool CreateTriangle()
        {
            static const char* const VertexSource = R"(#version 300 es
                const vec2 Positions[3] = vec2[](vec2(0.0, 0.62), vec2(-0.68, -0.58), vec2(0.68, -0.58));
                const vec3 Colors[3] = vec3[](vec3(1.0, 0.27, 0.23), vec3(0.19, 0.82, 0.35), vec3(0.04, 0.52, 1.0));
                out vec3 color;
                void main()
                {
                    gl_Position = vec4(Positions[gl_VertexID], 0.0, 1.0);
                    color = Colors[gl_VertexID];
                })";
            static const char* const FragmentSource = R"(#version 300 es
                precision mediump float;
                uniform float Brightness;
                in vec3 color;
                out vec4 fragmentColor;
                void main()
                {
                    fragmentColor = vec4(color * Brightness, 1.0);
                })";
            const GLuint vertex = m_GL.CreateShader(GlVertexShader);
            m_GL.ShaderSource(vertex, 1, &VertexSource, nullptr);
            m_GL.CompileShader(vertex);
            const GLuint fragment = m_GL.CreateShader(GlFragmentShader);
            m_GL.ShaderSource(fragment, 1, &FragmentSource, nullptr);
            m_GL.CompileShader(fragment);
            m_Program = m_GL.CreateProgram();
            m_GL.AttachShader(m_Program, vertex);
            m_GL.AttachShader(m_Program, fragment);
            m_GL.LinkProgram(m_Program);
            m_GL.DeleteShader(vertex);
            m_GL.DeleteShader(fragment);
            m_BrightnessLocation = m_GL.GetUniformLocation(m_Program, "Brightness");
            // A core profile draws nothing without a vertex array, even one without attributes.
            m_GL.GenVertexArrays(1, &m_VertexArray);
            return true;
        }

    private:
        GLFWwindow* m_Window = nullptr;
        HostFunctions m_GL;
        GLuint m_Program = 0;
        GLint m_BrightnessLocation = -1;
        GLuint m_VertexArray = 0;
        GLuint m_OffscreenTexture = 0;
        GLuint m_OffscreenFramebuffer = 0;
        int m_OffscreenWidth = 0;
        int m_OffscreenHeight = 0;
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
        Text("Carbon's interface and the host's triangle are drawn into the same OpenGL ES framebuffer.",
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
        Text("Drawn by the host in the same framebuffer", {.Style = TextStyle::Caption1, .Secondary = true});
        EndVStack();

        EndHStack();
    }
    /// Everything the frame loop needs. In a browser the loop is a callback, so this lives for the whole program.
    struct Application
    {
        Example::Arguments Arguments;
        OpenGLESHost Host;
        GLFWwindow* Window = nullptr;
        Carbon::Context* Context = nullptr;
        Settings Interface;
        float ContentScale = 1.0f;
        double LastTime = 0.0;
        int FrameIndex = 0;
        int ExitCode = 0;
    };

    /// Runs one frame. Returns false when the application should stop.
    bool RunFrame(Application& app)
    {
        const bool isScreenshot = !app.Arguments.ScreenshotPath.empty();
        float deltaTime = Example::ScreenshotDeltaTime;
        if (!isScreenshot)
        {
            glfwPollEvents();
            if (glfwWindowShouldClose(app.Window))
                return false;
            float scaleX = 1.0f;
            float scaleY = 1.0f;
            glfwGetWindowContentScale(app.Window, &scaleX, &scaleY);
            app.ContentScale = app.Arguments.Scale > 0.0f ? app.Arguments.Scale : scaleX;
            const double now = glfwGetTime();
            deltaTime = static_cast<float>(now - app.LastTime);
            app.LastTime = now;
        }
        else if (app.FrameIndex == Example::ScreenshotWarmupFrames)
        {
            return false;
        }
        int pixelWidth = 0;
        int pixelHeight = 0;
        app.Host.GetSize(pixelWidth, pixelHeight);
        if (pixelWidth == 0 || pixelHeight == 0)
            return true; // minimized

        // Display metrics and timing, every frame, before NewFrame.
        Carbon::IO& io = Carbon::GetIO();
        io.SetDisplaySize(static_cast<float>(pixelWidth) / app.ContentScale,
                          static_cast<float>(pixelHeight) / app.ContentScale);
        io.SetContentScale(app.ContentScale);
        io.SetDeltaTime(deltaTime);

        Carbon::NewFrame();
        BuildInterface(app.Interface);
        Carbon::EndFrame();

        // The framebuffer belongs to the host. It clears to the theme's background, draws its own content, then
        // Carbon's interface on top, into the framebuffer that is bound.
        app.Host.BeginFrame(Carbon::GetStyleColor(Carbon::StyleColor::Background));
        const Carbon::Rect area = app.Interface.HostArea;
        const float scale = app.ContentScale;
        app.Host.DrawTriangle(Carbon::Rect(std::round(area.X * scale), std::round(area.Y * scale),
                                           std::round(area.Width * scale), std::round(area.Height * scale)),
                              app.Interface.Brightness);
        Carbon::OpenGLESRender();
        app.Host.EndFrame();

        if (isScreenshot && app.FrameIndex == Example::ScreenshotWarmupFrames - 1 &&
            !app.Host.SaveScreenshot(app.Arguments, app.ContentScale))
            app.ExitCode = 1;
        app.FrameIndex++;
        return true;
    }

    /// Creates the window, the Carbon context and the backend. Returns false when that fails.
    bool Start(Application& app)
    {
        const Example::Arguments& arguments = app.Arguments;
        const bool isScreenshot = !arguments.ScreenshotPath.empty();
        const int width = arguments.Width > 0 ? arguments.Width : 760;
        const int height = arguments.Height > 0 ? arguments.Height : 440;

        // ---- 0. The host's window and OpenGL ES context ------------------------------------------------------------
        // Screenshot mode renders into a hidden window's offscreen framebuffer at a fixed scale.
        app.ContentScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
        if (!app.Host.Create(isScreenshot, static_cast<int>(std::lround(width * app.ContentScale)),
                             static_cast<int>(std::lround(height * app.ContentScale)), arguments.Scale <= 0.0f))
            return false;
        app.Window = isScreenshot ? nullptr : app.Host.GetWindow();

        // ---- 1. Create the Carbon context and install the OpenGL ES backend ----------------------------------
        Carbon::ContextDescription description;
        description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message)
        {
            const std::string_view levelName = Carbon::ToString(level);
            std::fprintf(stderr, "[%.*s] %.*s: %.*s\n", static_cast<int>(levelName.size()), levelName.data(),
                         static_cast<int>(source.size()), source.data(), static_cast<int>(message.size()),
                         message.data());
        };
        Example::InstallPlatformCallbacks(app.Window, description.Callbacks); // clipboard and cursor
        app.Context = Carbon::CreateContext(description);

        // The context is current; Carbon resolves its OpenGL ES functions through GLFW.
        Carbon::OpenGLESInitInfo info;
        info.GetProcAddress = &glfwGetProcAddress;
        info.ColorFormat = Carbon::TextureFormat::RGBA8Unorm; // the default framebuffer and a canvas are not sRGB
        if (!Carbon::OpenGLESInit(info))
            return false;

        app.Interface.IsDark = arguments.IsDark;
        Carbon::SetTheme(app.Interface.IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());

        // ---- 2. Forward the window's input events to Carbon --------------------------------------------------
        // WebGPUMinimal writes these callbacks out one by one; this is the same code.
        Example::InstallInputCallbacks(app.Window);
        app.LastTime = glfwGetTime();
        return true;
    }

#if !defined(__EMSCRIPTEN__)
    /// Shuts down while the OpenGL ES context is still current. A page never shuts down; it is closed.
    void Stop(Application& app)
    {
        Carbon::OpenGLESShutdown();
        Carbon::DestroyContext(app.Context);
        app.Context = nullptr;
    }
#endif
} // namespace

int main(int argc, char** argv)
{
    glfwSetErrorCallback([](int code, const char* description)
                         { std::fprintf(stderr, "GLFW error %d: %s\n", code, description); });
    if (!glfwInit())
        return 1;

    // ---- 3. The frame loop -------------------------------------------------------------------------------------
#if defined(__EMSCRIPTEN__)
    // The browser calls the frame function once per display refresh; main returns, the application lives on.
    static Application app;
    app.Arguments = Example::ParseArguments(argc, argv);
    if (!Start(app))
        return 1;
    emscripten_set_main_loop_arg([](void* data) { RunFrame(*static_cast<Application*>(data)); }, &app, 0, false);
    return 0;
#else
    int exitCode = 1;
    {
        Application app;
        app.Arguments = Example::ParseArguments(argc, argv);
        if (Start(app))
        {
            while (RunFrame(app))
            {
            }
            exitCode = app.ExitCode;
        }
        if (app.Context != nullptr)
            Stop(app);
    }
    glfwTerminate();
    return exitCode;
#endif
}
