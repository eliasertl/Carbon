// Minimal integration of Carbon into a host application that renders with OpenGL 3.3.
//
// The host owns the window, the OpenGL context and the framebuffer. Each frame it forwards input to Carbon, builds
// the interface, draws its own content and then lets Carbon add the interface on top, into the same framebuffer.
// Carbon saves and restores all OpenGL state it touches. The OpenGL chores (context, functions, the triangle) live
// in the OpenGLHost class below; everything Carbon-specific is in main(). WebGPUMinimalIntegration and
// VulkanMinimalIntegration do the same with the other backends.
//
// Carbon needs no OpenGL loader of the host's: it takes glfwGetProcAddress and resolves what it uses itself. This
// host loads its own few functions the same way; a real application would use glad or a similar loader.
//
//   OpenGLMinimalIntegration [--theme light|dark] [--scale <factor>] [--size <w>x<h>] [--screenshot <file.png>]

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Carbon/Backends/OpenGL/OpenGLBackend.h>
#include <Carbon/Carbon.h>

#include "ExampleArguments.h"
#include "GlfwInput.h"
#include "Screenshot.h"

namespace
{
    // ---------------------------------------------------------------------------------------------------------
    // The host's OpenGL. Nothing in here is specific to Carbon: an application already has all of it.
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

    /// The OpenGL functions this host uses, resolved through GLFW.
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

    class OpenGLHost
    {
    public:
        /// Creates the window (hidden in screenshot mode, which renders into an offscreen framebuffer of `width` x
        /// `height` pixels) and its OpenGL 3.3 core context.
        bool Create(bool isScreenshot, int width, int height, bool scaleToMonitor)
        {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
            glfwWindowHint(GLFW_VISIBLE, isScreenshot ? GLFW_FALSE : GLFW_TRUE);
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, scaleToMonitor ? GLFW_TRUE : GLFW_FALSE);
            m_Window = glfwCreateWindow(width, height, "Carbon - OpenGL Minimal Integration", nullptr, nullptr);
            if (m_Window == nullptr)
            {
                std::fprintf(stderr, "No OpenGL 3.3 core context could be created\n");
                return false;
            }
            glfwMakeContextCurrent(m_Window);
            glfwSwapInterval(1);
            if (!m_GL.LoadAll())
            {
                std::fprintf(stderr, "An OpenGL function could not be loaded\n");
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

        ~OpenGLHost()
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
            // OpenGL's viewport is measured from the bottom-left corner.
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
            // OpenGL's rows start at the bottom; PNG's at the top.
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
            static const char* const VertexSource = R"(#version 330 core
                const vec2 Positions[3] = vec2[](vec2(0.0, 0.62), vec2(-0.68, -0.58), vec2(0.68, -0.58));
                const vec3 Colors[3] = vec3[](vec3(1.0, 0.27, 0.23), vec3(0.19, 0.82, 0.35), vec3(0.04, 0.52, 1.0));
                out vec3 color;
                void main()
                {
                    gl_Position = vec4(Positions[gl_VertexID], 0.0, 1.0);
                    color = Colors[gl_VertexID];
                })";
            static const char* const FragmentSource = R"(#version 330 core
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
        Text("Carbon's interface and the host's triangle are drawn into the same OpenGL framebuffer.",
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
} // namespace

int main(int argc, char** argv)
{
    const Example::Arguments arguments = Example::ParseArguments(argc, argv);
    const bool isScreenshot = !arguments.ScreenshotPath.empty();
    const int width = arguments.Width > 0 ? arguments.Width : 760;
    const int height = arguments.Height > 0 ? arguments.Height : 440;

    // ---- 0. The host's window and OpenGL context ---------------------------------------------------------------
    // Screenshot mode renders into a hidden window's offscreen framebuffer at a fixed scale.
    glfwSetErrorCallback([](int code, const char* description)
                         { std::fprintf(stderr, "GLFW error %d: %s\n", code, description); });
    if (!glfwInit())
        return 1;
    float contentScale = arguments.Scale > 0.0f ? arguments.Scale : 1.0f;
    int exitCode = 0;
    {
        OpenGLHost host;
        if (!host.Create(isScreenshot, static_cast<int>(std::lround(width * contentScale)),
                         static_cast<int>(std::lround(height * contentScale)), arguments.Scale <= 0.0f))
        {
            glfwTerminate();
            return 1;
        }
        GLFWwindow* window = isScreenshot ? nullptr : host.GetWindow();

        // ---- 1. Create the Carbon context and install the OpenGL backend -------------------------------------
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

        // The context is current; Carbon resolves its OpenGL functions through GLFW.
        Carbon::OpenGLInitInfo info;
        info.GetProcAddress = &glfwGetProcAddress;
        info.ColorFormat = Carbon::TextureFormat::RGBA8Unorm; // the default framebuffer is not sRGB
        if (!Carbon::OpenGLInit(info))
            return 1;

        Settings settings;
        settings.IsDark = arguments.IsDark;
        Carbon::SetTheme(settings.IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());

        // ---- 2. Forward the window's input events to Carbon --------------------------------------------------
        // WebGPUMinimalIntegration writes these callbacks out one by one; this is the same code.
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
            }
            else if (frameIndex == Example::ScreenshotWarmupFrames)
            {
                break;
            }
            int pixelWidth = 0;
            int pixelHeight = 0;
            host.GetSize(pixelWidth, pixelHeight);
            if (pixelWidth == 0 || pixelHeight == 0)
            {
                glfwWaitEvents(); // minimized
                continue;
            }

            // Display metrics and timing, every frame, before NewFrame.
            Carbon::IO& io = Carbon::GetIO();
            io.SetDisplaySize(static_cast<float>(pixelWidth) / contentScale,
                              static_cast<float>(pixelHeight) / contentScale);
            io.SetContentScale(contentScale);
            io.SetDeltaTime(deltaTime);

            Carbon::NewFrame();
            BuildInterface(settings);
            Carbon::EndFrame();

            // The framebuffer belongs to the host. It clears to the theme's background, draws its own content, then
            // Carbon's interface on top, into the framebuffer that is bound.
            host.BeginFrame(Carbon::GetStyleColor(Carbon::StyleColor::Background));
            const Carbon::Rect area = settings.HostArea;
            host.DrawTriangle(
                Carbon::Rect(std::round(area.X * contentScale), std::round(area.Y * contentScale),
                             std::round(area.Width * contentScale), std::round(area.Height * contentScale)),
                settings.Brightness);
            Carbon::OpenGLRender();
            host.EndFrame();

            if (isScreenshot && frameIndex == Example::ScreenshotWarmupFrames - 1 &&
                !host.SaveScreenshot(arguments, contentScale))
                exitCode = 1;
        }

        // ---- 4. Shut down while the OpenGL context is still current ------------------------------------------
        Carbon::OpenGLShutdown();
        Carbon::DestroyContext(context);
    }
    glfwTerminate();
    return exitCode;
}
