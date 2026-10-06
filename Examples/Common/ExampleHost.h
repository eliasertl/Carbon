#pragma once

#include <webgpu/webgpu_cpp.h>

#include <Carbon/Backends/WebGPU/WebGPUBackend.h>
#include <Carbon/Carbon.h>

#include "ExampleArguments.h"

struct GLFWwindow;

namespace Example
{
    /// The parts of a host application that have nothing to do with Carbon: the window, the WebGPU instance,
    /// adapter and device, the surface, and the offscreen target used for screenshots.
    ///
    /// Carbon itself never does any of this; the examples keep it here so that each Main.cpp can concentrate on
    /// how Carbon is integrated.
    class Host
    {
    public:
        /// Creates the window (unless in screenshot mode) and the device. `width` and `height` are in points.
        Host(const Arguments& arguments, const char* title, int width, int height);
        ~Host();

        Host(const Host&) = delete;
        Host& operator=(const Host&) = delete;

        /// False when the window or the device could not be created; the reason was printed to stderr.
        bool IsReady() const { return m_IsReady; }
        bool IsScreenshotMode() const { return !m_Arguments.ScreenshotPath.empty(); }
        const Arguments& GetArguments() const { return m_Arguments; }

        /// The GLFW window; null in screenshot mode.
        GLFWwindow* GetWindow() const { return m_Window; }
        const wgpu::Device& GetDevice() const { return m_Device; }
        /// Format of the texture returned by GetTargetView.
        wgpu::TextureFormat GetColorFormat() const { return m_ColorFormat; }
        /// The same format in Carbon's terms, for WebGPUInitInfo::ColorFormat.
        Carbon::TextureFormat GetCarbonColorFormat() const;

        /// Pixels per point.
        float GetContentScale() const { return m_ContentScale; }
        /// Size of the render target in points.
        float GetWidth() const { return static_cast<float>(m_PixelWidth) / m_ContentScale; }
        float GetHeight() const { return static_cast<float>(m_PixelHeight) / m_ContentScale; }
        /// Seconds since the previous frame.
        float GetDeltaTime() const { return m_DeltaTime; }
        /// Number of frames finished so far.
        int GetFrameIndex() const { return m_FrameIndex; }

        /// Converts a GLFW cursor position (screen coordinates) to points.
        void CursorToPoints(double cursorX, double cursorY, float& x, float& y) const;

        /// Polls window events and acquires the texture to render into. Returns false when the application
        /// should exit (window closed, or screenshot taken).
        bool BeginFrame();
        /// The texture view to render this frame into. Valid between BeginFrame and EndFrame.
        const wgpu::TextureView& GetTargetView() const { return m_TargetView; }
        /// Presents the frame. In screenshot mode, saves the image after a few warm-up frames.
        void EndFrame();

        /// In screenshot mode, saves only `area` (in points) of the window, grown by --extend. Scripted pointer
        /// positions become relative to `anchor`. Examples call it every frame while what they crop to may move.
        void SetScreenshotArea(float x, float y, float width, float height, float anchorX, float anchorY);
        /// Where scripted pointer positions are measured from: the window's corner, or the anchor set above.
        float GetPointerOriginX() const { return m_PointerOriginX; }
        float GetPointerOriginY() const { return m_PointerOriginY; }

    private:
        bool CreateDevice();
        void ConfigureSurface();
        bool SaveScreenshot();

    private:
        Arguments m_Arguments;
        GLFWwindow* m_Window = nullptr;
        wgpu::Instance m_Instance;
        wgpu::Adapter m_Adapter;
        wgpu::Device m_Device;
        wgpu::Surface m_Surface;
        wgpu::Texture m_OffscreenTexture;
        wgpu::TextureView m_TargetView;
        wgpu::TextureFormat m_ColorFormat = wgpu::TextureFormat::BGRA8Unorm;

        uint32_t m_PixelWidth = 0;
        uint32_t m_PixelHeight = 0;
        uint32_t m_ConfiguredWidth = 0;
        uint32_t m_ConfiguredHeight = 0;
        float m_ContentScale = 1.0f;
        float m_DeltaTime = 1.0f / 60.0f;
        double m_LastTime = 0.0;
        int m_FrameIndex = 0;
        /// The area of the window a screenshot keeps, in points; empty for all of it.
        float m_Area[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float m_PointerOriginX = 0.0f;
        float m_PointerOriginY = 0.0f;
        bool m_IsReady = false;
        bool m_IsGlfwInitialized = false;
    };
} // namespace Example
