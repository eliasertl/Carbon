#pragma once

#include <EGL/egl.h>

#include "GraphicsDevice.h"

struct ANativeWindow;

namespace AndroidGallery
{
    /// The OpenGL ES 3 device of the Android Gallery: an EGL context and the window surface Android hands the app.
    ///
    /// On Android the surface comes and goes with the app's lifecycle (it is destroyed when the app goes to the
    /// background), and the context can be lost with it. The context is kept while only the surface is gone, so
    /// Carbon's backend and the app's textures survive; when the context itself is lost, the host installs Carbon's
    /// backend again and recreates its textures. It implements Example::GraphicsDevice for the Gallery's pages, which
    /// create their textures through it; the GLFW parts of that interface are not used.
    class AndroidDevice final : public Example::GraphicsDevice
    {
    public:
        AndroidDevice() = default;
        ~AndroidDevice() override;

        AndroidDevice(const AndroidDevice&) = delete;
        AndroidDevice& operator=(const AndroidDevice&) = delete;

        /// Creates the window surface for `window` and makes the context current on it, creating the context first
        /// when there is none. Sets `isNewContext` when the context was created now: Carbon's backend and the app's
        /// textures must then be created (again).
        bool CreateSurface(ANativeWindow* window, bool& isNewContext);
        /// Destroys the window surface and keeps the context, current without a surface.
        void DestroySurface();
        /// Destroys the surface and the context.
        void DestroyContext();
        /// True while there is a surface to render into.
        bool HasSurface() const { return m_Surface != EGL_NO_SURFACE; }
        /// True after a frame found the context lost (EGL_CONTEXT_LOST): the host shuts Carbon's backend down,
        /// destroys the context and creates everything again.
        bool IsContextLost() const { return m_IsContextLost; }
        /// The surface's current size in pixels; it follows rotation and resizing.
        void GetSurfaceSize(uint32_t& width, uint32_t& height) const;

        std::string_view GetName() const override { return "OpenGLES"; }
        void SetWindowHints() const override {}
        bool Create(GLFWwindow* window, bool isOffscreen, uint32_t width, uint32_t height) override;
        bool InitCarbon() override;
        void ShutdownCarbon() override;
        bool BeginFrame(uint32_t width, uint32_t height) override;
        void Render(Carbon::Color background) override;
        void EndFrame() override;
        bool ReadPixels(std::vector<uint8_t>& pixels) override;
        Carbon::TextureID CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override;

    private:
        bool CreateContext();

    private:
        EGLDisplay m_Display = EGL_NO_DISPLAY;
        EGLConfig m_Config = nullptr;
        EGLContext m_Context = EGL_NO_CONTEXT;
        EGLSurface m_Surface = EGL_NO_SURFACE;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        bool m_IsContextLost = false;
    };
} // namespace AndroidGallery
