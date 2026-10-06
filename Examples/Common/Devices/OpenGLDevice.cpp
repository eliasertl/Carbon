// The OpenGL device of the examples, for OpenGL 3.3 core or, built with CARBON_EXAMPLE_OPENGL_ES=1, for OpenGL ES 3.0
// and WebGL 2: the window's context and default framebuffer, or a hidden window's offscreen framebuffer for
// screenshots. OpenGLMinimal and OpenGLESMinimal show the same with the host's own drawing next to Carbon's.
//
// The few functions the host needs are loaded through GLFW, like Carbon's own; an application would use its loader.

#include <cstdio>
#include <cstring>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if CARBON_EXAMPLE_OPENGL_ES
#include <Carbon/Backends/OpenGLES/OpenGLESBackend.h>
#else
#include <Carbon/Backends/OpenGL/OpenGLBackend.h>
#endif

#include "GraphicsDevice.h"

#if defined(_WIN32) && !defined(_WIN64)
#define EXAMPLE_GL_CALL __stdcall
#else
#define EXAMPLE_GL_CALL
#endif

namespace Example
{
    namespace
    {
        using GLenum = unsigned int;
        using GLuint = unsigned int;
        using GLint = int;
        using GLsizei = int;
        using GLfloat = float;
        using GLbitfield = unsigned int;

        constexpr GLenum GlUnsignedByte = 0x1401;
        constexpr GLbitfield GlColorBufferBit = 0x4000;
        constexpr GLenum GlTexture2D = 0x0DE1;
        constexpr GLenum GlTextureMinFilter = 0x2801;
        constexpr GLenum GlTextureMagFilter = 0x2800;
        constexpr GLint GlLinear = 0x2601;
        constexpr GLenum GlRgba = 0x1908;
        constexpr GLenum GlRgba8 = 0x8058;
        constexpr GLenum GlFramebuffer = 0x8D40;
        constexpr GLenum GlColorAttachment0 = 0x8CE0;
        constexpr GLenum GlPackAlignment = 0x0D05;
        constexpr GLenum GlUnpackAlignment = 0x0CF5;

        /// The OpenGL functions this host uses, resolved through GLFW.
        struct HostFunctions
        {
            void(EXAMPLE_GL_CALL* Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
            void(EXAMPLE_GL_CALL* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
            void(EXAMPLE_GL_CALL* Clear)(GLbitfield) = nullptr;
            void(EXAMPLE_GL_CALL* GenTextures)(GLsizei, GLuint*) = nullptr;
            void(EXAMPLE_GL_CALL* DeleteTextures)(GLsizei, const GLuint*) = nullptr;
            void(EXAMPLE_GL_CALL* BindTexture)(GLenum, GLuint) = nullptr;
            void(EXAMPLE_GL_CALL* TexParameteri)(GLenum, GLenum, GLint) = nullptr;
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
                       Load(GenTextures, "glGenTextures") && Load(DeleteTextures, "glDeleteTextures") &&
                       Load(BindTexture, "glBindTexture") && Load(TexParameteri, "glTexParameteri") &&
                       Load(TexImage2D, "glTexImage2D") && Load(GenFramebuffers, "glGenFramebuffers") &&
                       Load(DeleteFramebuffers, "glDeleteFramebuffers") && Load(BindFramebuffer, "glBindFramebuffer") &&
                       Load(FramebufferTexture2D, "glFramebufferTexture2D") && Load(PixelStorei, "glPixelStorei") &&
                       Load(ReadPixels, "glReadPixels");
            }
        };

        class OpenGLDevice : public GraphicsDevice
        {
        public:
            ~OpenGLDevice() override
            {
                if (m_Window == nullptr)
                    return;
                glfwMakeContextCurrent(m_Window);
                if (!m_Textures.empty())
                    m_GL.DeleteTextures(static_cast<GLsizei>(m_Textures.size()), m_Textures.data());
                if (m_OffscreenFramebuffer != 0)
                {
                    m_GL.DeleteFramebuffers(1, &m_OffscreenFramebuffer);
                    m_GL.DeleteTextures(1, &m_OffscreenTexture);
                }
            }

#if CARBON_EXAMPLE_OPENGL_ES
            std::string_view GetName() const override { return "OpenGLES"; }

            void SetWindowHints() const override
            {
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
            }
#else
            std::string_view GetName() const override { return "OpenGL"; }

            void SetWindowHints() const override
            {
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
            }
#endif

            bool NeedsWindowOffscreen() const override { return true; }

            bool Create(GLFWwindow* window, bool isOffscreen, uint32_t width, uint32_t height) override
            {
                m_Window = window;
                m_Width = static_cast<GLsizei>(width);
                m_Height = static_cast<GLsizei>(height);
                glfwMakeContextCurrent(m_Window);
                glfwSwapInterval(1);
                if (!m_GL.LoadAll())
                {
                    std::fprintf(stderr, "An OpenGL function could not be loaded\n");
                    return false;
                }
                if (isOffscreen)
                {
                    m_GL.GenTextures(1, &m_OffscreenTexture);
                    m_GL.BindTexture(GlTexture2D, m_OffscreenTexture);
                    m_GL.TexImage2D(GlTexture2D, 0, static_cast<GLint>(GlRgba8), m_Width, m_Height, 0, GlRgba,
                                    GlUnsignedByte, nullptr);
                    m_GL.BindTexture(GlTexture2D, 0);
                    m_GL.GenFramebuffers(1, &m_OffscreenFramebuffer);
                    m_GL.BindFramebuffer(GlFramebuffer, m_OffscreenFramebuffer);
                    m_GL.FramebufferTexture2D(GlFramebuffer, GlColorAttachment0, GlTexture2D, m_OffscreenTexture, 0);
                }
                return true;
            }

            bool InitCarbon() override
            {
                // The context is current; Carbon resolves its functions through GLFW. The default framebuffer, a
                // canvas and the offscreen texture are not sRGB.
#if CARBON_EXAMPLE_OPENGL_ES
                Carbon::OpenGLESInitInfo info;
                info.GetProcAddress = &glfwGetProcAddress;
                info.ColorFormat = Carbon::TextureFormat::RGBA8Unorm;
                return Carbon::OpenGLESInit(info);
#else
                Carbon::OpenGLInitInfo info;
                info.GetProcAddress = &glfwGetProcAddress;
                info.ColorFormat = Carbon::TextureFormat::RGBA8Unorm;
                return Carbon::OpenGLInit(info);
#endif
            }

            void ShutdownCarbon() override
            {
#if CARBON_EXAMPLE_OPENGL_ES
                Carbon::OpenGLESShutdown();
#else
                Carbon::OpenGLShutdown();
#endif
            }

            bool BeginFrame(uint32_t width, uint32_t height) override
            {
                if (m_OffscreenFramebuffer == 0)
                {
                    m_Width = static_cast<GLsizei>(width);
                    m_Height = static_cast<GLsizei>(height);
                }
                return true;
            }

            void Render(Carbon::Color background) override
            {
                m_GL.BindFramebuffer(GlFramebuffer, m_OffscreenFramebuffer);
                m_GL.Viewport(0, 0, m_Width, m_Height);
                m_GL.ClearColor(background.R, background.G, background.B, 1.0f);
                m_GL.Clear(GlColorBufferBit);
                // Carbon draws into the bound framebuffer.
#if CARBON_EXAMPLE_OPENGL_ES
                Carbon::OpenGLESRender();
#else
                Carbon::OpenGLRender();
#endif
            }

            void EndFrame() override
            {
#if !defined(__EMSCRIPTEN__) // a browser presents the canvas after each frame by itself
                if (m_OffscreenFramebuffer == 0)
                    glfwSwapBuffers(m_Window);
#endif
            }

            bool ReadPixels(std::vector<uint8_t>& pixels) override
            {
                const size_t rowBytes = static_cast<size_t>(m_Width) * 4;
                std::vector<uint8_t> rows(rowBytes * static_cast<size_t>(m_Height));
                m_GL.BindFramebuffer(GlFramebuffer, m_OffscreenFramebuffer);
                m_GL.PixelStorei(GlPackAlignment, 1);
                m_GL.ReadPixels(0, 0, m_Width, m_Height, GlRgba, GlUnsignedByte, rows.data());
                // OpenGL's rows start at the bottom.
                pixels.resize(rows.size());
                for (GLsizei row = 0; row < m_Height; row++)
                    std::memcpy(&pixels[static_cast<size_t>(row) * rowBytes],
                                &rows[static_cast<size_t>(m_Height - 1 - row) * rowBytes], rowBytes);
                return true;
            }

            Carbon::TextureID CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override
            {
                GLuint texture = 0;
                m_GL.GenTextures(1, &texture);
                m_GL.BindTexture(GlTexture2D, texture);
                m_GL.TexParameteri(GlTexture2D, GlTextureMinFilter, GlLinear);
                m_GL.TexParameteri(GlTexture2D, GlTextureMagFilter, GlLinear);
                m_GL.PixelStorei(GlUnpackAlignment, 1);
                m_GL.TexImage2D(GlTexture2D, 0, static_cast<GLint>(GlRgba8), static_cast<GLsizei>(width),
                                static_cast<GLsizei>(height), 0, GlRgba, GlUnsignedByte, texels.data());
                m_GL.BindTexture(GlTexture2D, 0);
                m_Textures.push_back(texture);
                // Rows uploaded from memory: row 0 is the top of the image, as Carbon samples it.
                return Carbon::MakeTextureID(texture);
            }

        private:
            GLFWwindow* m_Window = nullptr;
            HostFunctions m_GL;
            GLuint m_OffscreenTexture = 0;
            GLuint m_OffscreenFramebuffer = 0;
            std::vector<GLuint> m_Textures;
            GLsizei m_Width = 0;
            GLsizei m_Height = 0;
        };
    } // namespace

    std::unique_ptr<GraphicsDevice> CreateGraphicsDevice()
    {
        return std::make_unique<OpenGLDevice>();
    }
} // namespace Example
