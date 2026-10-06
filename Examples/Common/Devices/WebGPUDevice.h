#pragma once

#include <webgpu/webgpu_cpp.h>

#include <Carbon/Backends/WebGPU/WebGPUBackend.h>

#include "GraphicsDevice.h"

namespace Example
{
    /// The WebGPU (Dawn) device of the examples: instance, adapter, device, the window's surface or an offscreen
    /// texture. WebGPUMinimal records its own render pass and uses the getters below; the other examples go through
    /// GraphicsDevice.
    class WebGPUDevice : public GraphicsDevice
    {
    public:
        std::string_view GetName() const override { return "WebGPU"; }
        void SetWindowHints() const override;
        bool Create(GLFWwindow* window, bool isOffscreen, uint32_t width, uint32_t height) override;
        bool InitCarbon() override;
        void ShutdownCarbon() override;
        bool BeginFrame(uint32_t width, uint32_t height) override;
        void Render(Carbon::Color background) override;
        void EndFrame() override;
        bool ReadPixels(std::vector<uint8_t>& pixels) override;
        Carbon::TextureID CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) override;

        const wgpu::Device& GetDevice() const { return m_Device; }
        /// Format of the texture returned by GetTargetView.
        wgpu::TextureFormat GetColorFormat() const { return m_ColorFormat; }
        /// The same format in Carbon's terms, for WebGPUInitInfo::ColorFormat.
        Carbon::TextureFormat GetCarbonColorFormat() const;
        /// The texture view to render this frame into. Valid between BeginFrame and EndFrame.
        const wgpu::TextureView& GetTargetView() const { return m_TargetView; }

    private:
        void ConfigureSurface();

    private:
        wgpu::Instance m_Instance;
        wgpu::Adapter m_Adapter;
        wgpu::Device m_Device;
        wgpu::Surface m_Surface;
        wgpu::Texture m_OffscreenTexture;
        wgpu::TextureView m_TargetView;
        wgpu::TextureFormat m_ColorFormat = wgpu::TextureFormat::BGRA8Unorm;
        std::vector<wgpu::TextureView> m_Textures;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_ConfiguredWidth = 0;
        uint32_t m_ConfiguredHeight = 0;
    };
} // namespace Example
