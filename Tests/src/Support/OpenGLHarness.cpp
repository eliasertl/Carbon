#include "Support/BackendHarness.h"

#if defined(CARBON_HAS_BACKEND_OPENGL) || defined(CARBON_HAS_BACKEND_OPENGLES)

#include <cstring>
#include <format>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Carbon/Backends/OpenGL/OpenGLFunctionsInternal.h"
#if defined(CARBON_HAS_BACKEND_OPENGL)
#include "Carbon/Backends/OpenGL/OpenGLBackend.h"
#endif
#if defined(CARBON_HAS_BACKEND_OPENGLES)
#include "Carbon/Backends/OpenGLES/OpenGLESBackend.h"
#endif

namespace Carbon
{
    namespace
    {
        using namespace Carbon::Internal;

        // What the harness needs beyond the functions Carbon's backend resolves.
        constexpr GLenum Framebuffer = 0x8D40;
        constexpr GLenum ColorAttachment0 = 0x8CE0;
        constexpr GLenum FramebufferComplete = 0x8CD5;
        constexpr GLenum ColorBufferBit = 0x4000;
        constexpr GLenum Rgba = 0x1908;
        constexpr GLenum Rgba8 = 0x8058;
        constexpr GLenum Srgb8Alpha8 = 0x8C43;
        constexpr GLenum DebugOutputSynchronous = 0x8242;
        constexpr GLenum DebugTypeError = 0x824C;
        constexpr GLenum DebugTypeUndefinedBehavior = 0x824E;
        constexpr GLenum DebugSeverityHigh = 0x9146;
        constexpr GLenum DebugSeverityMedium = 0x9147;

        using DebugCallback = void(CB_OPENGL_CALL*)(GLenum source, GLenum type, GLuint id, GLenum severity,
                                                    GLsizei length, const GLchar* message, const void* user);

        struct HarnessFunctions
        {
            void(CB_OPENGL_CALL* GenFramebuffers)(GLsizei, GLuint*) = nullptr;
            void(CB_OPENGL_CALL* DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
            void(CB_OPENGL_CALL* BindFramebuffer)(GLenum, GLuint) = nullptr;
            void(CB_OPENGL_CALL* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
            GLenum(CB_OPENGL_CALL* CheckFramebufferStatus)(GLenum) = nullptr;
            void(CB_OPENGL_CALL* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
            void(CB_OPENGL_CALL* Clear)(GLbitfield) = nullptr;
            void(CB_OPENGL_CALL* ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
            void(CB_OPENGL_CALL* Finish)() = nullptr;
            void(CB_OPENGL_CALL* DebugMessageCallback)(DebugCallback, const void*) = nullptr;
        };

        template <typename Function>
        void Load(Function& function, const char* name)
        {
            function = reinterpret_cast<Function>(glfwGetProcAddress(name));
        }

        /// A piece of OpenGL state the host might have set, which OpenGLRender must leave as it found it.
        struct HostState
        {
            GLint Program = 0;
            GLint ActiveTexture = 0;
            GLint Viewport[4] = {};
            GLint ScissorBox[4] = {};
            GLint BlendSrcRgb = 0;
            GLboolean Blend = 0;
            GLboolean ScissorTest = 0;
            GLboolean CullFace = 0;
            GLboolean FramebufferSrgb = 0;
            GLint UnpackAlignment = 0;

            bool operator==(const HostState& other) const
            {
                return Program == other.Program && ActiveTexture == other.ActiveTexture &&
                       std::memcmp(Viewport, other.Viewport, sizeof(Viewport)) == 0 &&
                       std::memcmp(ScissorBox, other.ScissorBox, sizeof(ScissorBox)) == 0 &&
                       BlendSrcRgb == other.BlendSrcRgb && Blend == other.Blend && ScissorTest == other.ScissorTest &&
                       CullFace == other.CullFace && FramebufferSrgb == other.FramebufferSrgb &&
                       UnpackAlignment == other.UnpackAlignment;
            }
        };

        /// Drives the OpenGL backend in a hidden GLFW window with an OpenGL 3.3 core debug context, or the OpenGL ES
        /// backend with an OpenGL ES 3.0 context (`isES`), rendering into a framebuffer object. Debug output of type
        /// error or undefined behavior, OpenGL errors and any state the backend fails to restore are collected as
        /// messages.
        class OpenGLHarness : public BackendHarness
        {
        public:
            explicit OpenGLHarness(bool isES) : m_IsES(isES) {}

            ~OpenGLHarness() override
            {
                if (m_Window == nullptr)
                    return;
                glfwMakeContextCurrent(m_Window);
                for (GLuint texture : m_Textures)
                    m_GL.DeleteTextures(1, &texture);
                glfwDestroyWindow(m_Window);
            }

            std::string_view GetName() const override { return m_IsES ? "OpenGLES" : "OpenGL"; }

            std::string CreateDevice() override
            {
                // GLFW stays initialized for the rest of the run; several harnesses may exist at once.
                static const bool IsGlfwReady = glfwInit() == GLFW_TRUE;
                if (!IsGlfwReady)
                    return "GLFW could not be initialized (no display?)";

                glfwDefaultWindowHints();
                glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                glfwWindowHint(GLFW_CLIENT_API, m_IsES ? GLFW_OPENGL_ES_API : GLFW_OPENGL_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_IsES ? 0 : 3);
                if (!m_IsES)
                {
                    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
                }
                glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
                m_Window = glfwCreateWindow(64, 64, "CarbonTests", nullptr, nullptr);
                if (m_Window == nullptr)
                    return m_IsES ? "No OpenGL ES 3.0 context could be created"
                                  : "No OpenGL 3.3 core context could be created";
                glfwMakeContextCurrent(m_Window);

                std::string_view missing;
                if (!m_GL.Load(&glfwGetProcAddress, m_IsES, missing))
                    return std::format("The OpenGL function {} is missing", missing);
                Load(m_Extra.GenFramebuffers, "glGenFramebuffers");
                Load(m_Extra.DeleteFramebuffers, "glDeleteFramebuffers");
                Load(m_Extra.BindFramebuffer, "glBindFramebuffer");
                Load(m_Extra.FramebufferTexture2D, "glFramebufferTexture2D");
                Load(m_Extra.CheckFramebufferStatus, "glCheckFramebufferStatus");
                Load(m_Extra.ClearColor, "glClearColor");
                Load(m_Extra.Clear, "glClear");
                Load(m_Extra.ReadPixels, "glReadPixels");
                Load(m_Extra.Finish, "glFinish");
                Load(m_Extra.DebugMessageCallback, "glDebugMessageCallback");

                // Debug output needs OpenGL 4.3 or KHR_debug; without it, glGetError still catches errors.
                if (m_Extra.DebugMessageCallback != nullptr)
                {
                    m_GL.Enable(DebugOutputSynchronous);
                    m_Extra.DebugMessageCallback(
                        [](GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* message,
                           const void* user)
                        {
                            if (type == DebugTypeError || type == DebugTypeUndefinedBehavior ||
                                severity == DebugSeverityHigh || severity == DebugSeverityMedium)
                            {
                                static_cast<OpenGLHarness*>(const_cast<void*>(user))->m_Messages.emplace_back(message);
                            }
                        },
                        this);
                }
                return {};
            }

            bool InitBackend(TextureFormat colorFormat) override
            {
                glfwMakeContextCurrent(m_Window);
                m_IsSrgb = IsSrgbFormat(colorFormat);
#if defined(CARBON_HAS_BACKEND_OPENGLES)
                if (m_IsES)
                {
                    OpenGLESInitInfo info;
                    info.GetProcAddress = &glfwGetProcAddress;
                    info.ColorFormat = colorFormat;
                    return OpenGLESInit(info);
                }
#endif
#if defined(CARBON_HAS_BACKEND_OPENGL)
                OpenGLInitInfo info;
                info.GetProcAddress = &glfwGetProcAddress;
                info.ColorFormat = colorFormat;
                return OpenGLInit(info);
#else
                return false;
#endif
            }

            void ShutdownBackend() override
            {
                glfwMakeContextCurrent(m_Window);
#if defined(CARBON_HAS_BACKEND_OPENGLES)
                if (m_IsES)
                {
                    OpenGLESShutdown();
                    return;
                }
#endif
#if defined(CARBON_HAS_BACKEND_OPENGL)
                OpenGLShutdown();
#endif
            }

            RenderedImage RenderFrame(uint32_t width, uint32_t height, Color background) override
            {
                glfwMakeContextCurrent(m_Window);
                RenderedImage image;
                image.Width = width;
                image.Height = height;

                GLuint texture = 0;
                m_GL.GenTextures(1, &texture);
                m_GL.BindTexture(GL::Texture2D, texture);
                m_GL.TexImage2D(GL::Texture2D, 0, static_cast<GLint>(m_IsSrgb ? Srgb8Alpha8 : Rgba8),
                                static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, Rgba, GL::UnsignedByte,
                                nullptr);
                GLuint framebuffer = 0;
                m_Extra.GenFramebuffers(1, &framebuffer);
                m_Extra.BindFramebuffer(Framebuffer, framebuffer);
                m_Extra.FramebufferTexture2D(Framebuffer, ColorAttachment0, GL::Texture2D, texture, 0);
                if (m_Extra.CheckFramebufferStatus(Framebuffer) != FramebufferComplete)
                    m_Messages.emplace_back("The framebuffer is incomplete");

                // The clear color is linear for an sRGB target, as in the other APIs. OpenGL ES always encodes.
                if (m_IsSrgb && !m_IsES)
                    m_GL.Enable(GL::FramebufferSrgb);
                m_GL.Viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
                m_Extra.ClearColor(background.R, background.G, background.B, background.A);
                m_Extra.Clear(ColorBufferBit);

                // State a host might have left: OpenGLRender must restore all of it.
                m_GL.Viewport(5, 6, 7, 8);
                m_GL.Enable(GL::ScissorTest);
                m_GL.Scissor(1, 2, 3, 4);
                m_GL.Enable(GL::CullFace);
                m_GL.Disable(GL::Blend);
                m_GL.BlendFuncSeparate(GL::One, GL::One, GL::One, GL::One);
                if (!m_IsES)
                    m_GL.Enable(GL::FramebufferSrgb);
                m_GL.ActiveTexture(GL::Texture0 + 3);
                m_GL.PixelStorei(GL::UnpackAlignment, 8);
                const HostState before = ReadHostState();
                RenderWithBackend();
                if (!(ReadHostState() == before))
                    m_Messages.emplace_back("OpenGLRender did not restore the host's OpenGL state");
                m_GL.Disable(GL::ScissorTest);
                m_GL.Disable(GL::CullFace);
                if (!m_IsES)
                    m_GL.Disable(GL::FramebufferSrgb);
                m_GL.ActiveTexture(GL::Texture0);
                m_GL.PixelStorei(GL::UnpackAlignment, 4);

                // OpenGL's rows start at the bottom.
                m_GL.PixelStorei(0x0D05, 1); // GL_PACK_ALIGNMENT
                std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
                m_Extra.ReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), Rgba,
                                   GL::UnsignedByte, pixels.data());
                image.Pixels.resize(pixels.size());
                const size_t rowBytes = static_cast<size_t>(width) * 4;
                for (uint32_t row = 0; row < height; row++)
                {
                    std::memcpy(&image.Pixels[row * rowBytes], &pixels[(height - 1 - row) * rowBytes], rowBytes);
                }

                m_Extra.BindFramebuffer(Framebuffer, 0);
                m_Extra.DeleteFramebuffers(1, &framebuffer);
                m_GL.DeleteTextures(1, &texture);
                CheckErrors();
                return image;
            }

            size_t CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
            {
                glfwMakeContextCurrent(m_Window);
                GLuint texture = 0;
                m_GL.GenTextures(1, &texture);
                m_GL.BindTexture(GL::Texture2D, texture);
                m_GL.PixelStorei(GL::UnpackAlignment, 1);
                m_GL.TexImage2D(GL::Texture2D, 0, static_cast<GLint>(Rgba8), static_cast<GLsizei>(width),
                                static_cast<GLsizei>(height), 0, Rgba, GL::UnsignedByte, texels.data());
                m_GL.TexParameteri(GL::Texture2D, GL::TextureMaxLevel, 0);
                m_GL.PixelStorei(GL::UnpackAlignment, 4);
                m_GL.BindTexture(GL::Texture2D, 0);
                m_Textures.push_back(texture);
                return m_Textures.size() - 1;
            }

            TextureID GetTextureID(size_t texture) override
            {
#if defined(CARBON_HAS_BACKEND_OPENGLES)
                if (m_IsES)
                    return OpenGLESGetTextureID(m_Textures[texture]);
#endif
#if defined(CARBON_HAS_BACKEND_OPENGL)
                return OpenGLGetTextureID(m_Textures[texture]);
#else
                return TextureID();
#endif
            }

            TextureID GetRawTextureID(size_t texture) override { return MakeTextureID(m_Textures[texture]); }

            std::vector<std::string> TakeMessages() override
            {
                if (m_Window != nullptr)
                {
                    glfwMakeContextCurrent(m_Window);
                    CheckErrors();
                }
                std::vector<std::string> messages = std::move(m_Messages);
                m_Messages.clear();
                return messages;
            }

        private:
            void RenderWithBackend()
            {
#if defined(CARBON_HAS_BACKEND_OPENGLES)
                if (m_IsES)
                {
                    OpenGLESRender();
                    return;
                }
#endif
#if defined(CARBON_HAS_BACKEND_OPENGL)
                OpenGLRender();
#endif
            }

            HostState ReadHostState() const
            {
                HostState state;
                m_GL.GetIntegerv(GL::CurrentProgram, &state.Program);
                m_GL.GetIntegerv(GL::ActiveTexture, &state.ActiveTexture);
                m_GL.GetIntegerv(GL::Viewport, state.Viewport);
                m_GL.GetIntegerv(GL::ScissorBox, state.ScissorBox);
                m_GL.GetIntegerv(GL::BlendSrcRgb, &state.BlendSrcRgb);
                m_GL.GetIntegerv(GL::UnpackAlignment, &state.UnpackAlignment);
                state.Blend = m_GL.IsEnabled(GL::Blend);
                state.ScissorTest = m_GL.IsEnabled(GL::ScissorTest);
                state.CullFace = m_GL.IsEnabled(GL::CullFace);
                if (!m_IsES)
                    state.FramebufferSrgb = m_GL.IsEnabled(GL::FramebufferSrgb);
                return state;
            }

            void CheckErrors()
            {
                for (GLenum error = m_GL.GetError(); error != GL::NoError; error = m_GL.GetError())
                    m_Messages.push_back(std::format("glGetError reported {:#x}", error));
            }

        private:
            bool m_IsES = false;
            GLFWwindow* m_Window = nullptr;
            OpenGLFunctions m_GL;
            HarnessFunctions m_Extra;
            bool m_IsSrgb = false;
            std::vector<GLuint> m_Textures;
            std::vector<std::string> m_Messages;
        };
    } // namespace

    std::unique_ptr<BackendHarness> CreateOpenGLHarness(bool isES)
    {
        return std::make_unique<OpenGLHarness>(isES);
    }
} // namespace Carbon

#endif
