#include "AndroidDevice.h"

#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window.h>

#include <Carbon/Backends/OpenGLES/OpenGLESBackend.h>

namespace AndroidGallery
{
    namespace
    {
        void LogError(const char* what)
        {
            __android_log_print(ANDROID_LOG_ERROR, "CarbonGallery", "%s (EGL error 0x%x)", what, eglGetError());
        }
    } // namespace

    AndroidDevice::~AndroidDevice()
    {
        DestroyContext();
    }

    bool AndroidDevice::CreateContext()
    {
        if (m_Display == EGL_NO_DISPLAY)
        {
            m_Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (m_Display == EGL_NO_DISPLAY || eglInitialize(m_Display, nullptr, nullptr) != EGL_TRUE)
            {
                LogError("The EGL display could not be initialized");
                m_Display = EGL_NO_DISPLAY;
                return false;
            }
        }

        // RGBA8 without depth or stencil: Carbon needs neither. The framebuffer is not sRGB, so Carbon writes
        // sRGB-encoded values as it does on the other backends' default formats.
        const EGLint configAttributes[] = {EGL_RENDERABLE_TYPE,
                                           EGL_OPENGL_ES3_BIT,
                                           EGL_SURFACE_TYPE,
                                           EGL_WINDOW_BIT,
                                           EGL_RED_SIZE,
                                           8,
                                           EGL_GREEN_SIZE,
                                           8,
                                           EGL_BLUE_SIZE,
                                           8,
                                           EGL_ALPHA_SIZE,
                                           8,
                                           EGL_NONE};
        EGLint count = 0;
        if (eglChooseConfig(m_Display, configAttributes, &m_Config, 1, &count) != EGL_TRUE || count < 1)
        {
            LogError("No EGL configuration for OpenGL ES 3 with RGBA8");
            return false;
        }
        const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
        m_Context = eglCreateContext(m_Display, m_Config, EGL_NO_CONTEXT, contextAttributes);
        if (m_Context == EGL_NO_CONTEXT)
        {
            LogError("The OpenGL ES 3 context could not be created");
            return false;
        }
        m_IsContextLost = false;
        return true;
    }

    bool AndroidDevice::CreateSurface(ANativeWindow* window, bool& isNewContext)
    {
        isNewContext = false;
        if (m_Context == EGL_NO_CONTEXT)
        {
            if (!CreateContext())
                return false;
            isNewContext = true;
        }
        DestroySurface();
        m_Surface = eglCreateWindowSurface(m_Display, m_Config, window, nullptr);
        if (m_Surface == EGL_NO_SURFACE)
        {
            LogError("The window surface could not be created");
            return false;
        }
        if (eglMakeCurrent(m_Display, m_Surface, m_Surface, m_Context) != EGL_TRUE)
        {
            LogError("The OpenGL ES context could not be made current");
            return false;
        }
        // One frame per refresh of the display.
        eglSwapInterval(m_Display, 1);
        return true;
    }

    void AndroidDevice::DestroySurface()
    {
        if (m_Display == EGL_NO_DISPLAY)
            return;
        // The context stays current without a surface, so that Carbon's backend can still be shut down.
        eglMakeCurrent(m_Display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_Context);
        if (m_Surface != EGL_NO_SURFACE)
            eglDestroySurface(m_Display, m_Surface);
        m_Surface = EGL_NO_SURFACE;
    }

    void AndroidDevice::DestroyContext()
    {
        if (m_Display == EGL_NO_DISPLAY)
            return;
        DestroySurface();
        eglMakeCurrent(m_Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_Context != EGL_NO_CONTEXT)
            eglDestroyContext(m_Display, m_Context);
        m_Context = EGL_NO_CONTEXT;
        m_IsContextLost = false;
        eglTerminate(m_Display);
        m_Display = EGL_NO_DISPLAY;
    }

    void AndroidDevice::GetSurfaceSize(uint32_t& width, uint32_t& height) const
    {
        EGLint surfaceWidth = 0;
        EGLint surfaceHeight = 0;
        if (m_Surface != EGL_NO_SURFACE)
        {
            eglQuerySurface(m_Display, m_Surface, EGL_WIDTH, &surfaceWidth);
            eglQuerySurface(m_Display, m_Surface, EGL_HEIGHT, &surfaceHeight);
        }
        width = static_cast<uint32_t>(surfaceWidth > 0 ? surfaceWidth : 0);
        height = static_cast<uint32_t>(surfaceHeight > 0 ? surfaceHeight : 0);
    }

    bool AndroidDevice::Create(GLFWwindow*, bool, uint32_t, uint32_t)
    {
        // The desktop host's entry point; on Android the surface comes from CreateSurface.
        return false;
    }

    bool AndroidDevice::InitCarbon()
    {
        // eglGetProcAddress returns core functions too since EGL 1.5, and on every Android version.
        return Carbon::OpenGLESInit(
            {.GetProcAddress = reinterpret_cast<Carbon::OpenGLESGetProcAddress>(&eglGetProcAddress)});
    }

    void AndroidDevice::ShutdownCarbon()
    {
        Carbon::OpenGLESShutdown();
    }

    bool AndroidDevice::BeginFrame(uint32_t width, uint32_t height)
    {
        if (m_Surface == EGL_NO_SURFACE || m_IsContextLost || width == 0 || height == 0)
            return false;
        m_Width = width;
        m_Height = height;
        return true;
    }

    void AndroidDevice::Render(Carbon::Color background)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
        glClearColor(background.R, background.G, background.B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Carbon::OpenGLESRender();
    }

    void AndroidDevice::EndFrame()
    {
        if (eglSwapBuffers(m_Display, m_Surface) == EGL_TRUE)
            return;
        const EGLint error = eglGetError();
        if (error == EGL_CONTEXT_LOST)
            m_IsContextLost = true;
        else if (error == EGL_BAD_SURFACE || error == EGL_BAD_NATIVE_WINDOW)
            DestroySurface(); // the window went away; the next APP_CMD_INIT_WINDOW brings a new one
    }

    bool AndroidDevice::ReadPixels(std::vector<uint8_t>&)
    {
        // Screenshots of the app come from Android itself (adb exec-out screencap).
        return false;
    }

    Carbon::TextureID AndroidDevice::CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels)
    {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, texels.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        return Carbon::OpenGLESGetTextureID(texture);
    }
} // namespace AndroidGallery
