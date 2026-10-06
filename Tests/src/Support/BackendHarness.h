#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Carbon/Carbon.h"

namespace Carbon
{
    /// An image read back from the GPU: 8-bit RGBA, row by row from the top.
    struct RenderedImage
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<uint8_t> Pixels;

        /// The color of one pixel.
        Color GetPixel(uint32_t x, uint32_t y) const
        {
            const uint8_t* pixel = &Pixels[(static_cast<size_t>(y) * Width + x) * 4];
            return Color::FromRGBA8(pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    };

    /// Drives one of Carbon's renderer backends without a window: creates a device of its API, installs the
    /// backend into the current context, renders frames offscreen and reads them back. One implementation per
    /// backend that is compiled in; the tests are written once against this class.
    class BackendHarness
    {
    public:
        virtual ~BackendHarness() = default;

        /// The backend's name, as in GetCompiledBackends.
        virtual std::string_view GetName() const = 0;

        /// Creates the instance and device. Returns why that is not possible on this machine, or an empty string.
        virtual std::string CreateDevice() = 0;

        /// Installs Carbon's backend for this API into the current context, for targets of `colorFormat`.
        virtual bool InitBackend(TextureFormat colorFormat) = 0;

        /// Removes the backend again, through the backend's own shutdown function.
        virtual void ShutdownBackend() = 0;

        /// Renders the current context's last finished frame into a new target of `width` x `height` pixels in
        /// the format given to InitBackend, cleared to `background`, and reads it back.
        virtual RenderedImage RenderFrame(uint32_t width, uint32_t height, Color background) = 0;

        /// Creates an RGBA8 texture from `texels` (rows from the top) and returns a handle for GetTextureID. The
        /// texture lives as long as the harness.
        virtual size_t CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) = 0;

        /// Registers a texture made by CreateTexture with the backend and returns its ID.
        virtual TextureID GetTextureID(size_t texture) = 0;

        /// Errors and warnings the API reported (validation layers, debug output) since the last call.
        virtual std::vector<std::string> TakeMessages() = 0;
    };

    /// The names of the backends compiled into Carbon, WebGPU first (it is the reference for comparisons).
    std::vector<std::string> GetCompiledBackends();

    /// Creates the harness for a backend from GetCompiledBackends.
    std::unique_ptr<BackendHarness> CreateBackendHarness(std::string_view name);

#if defined(CARBON_HAS_BACKEND_WEBGPU)
    std::unique_ptr<BackendHarness> CreateWebGPUHarness();
#endif
#if defined(CARBON_HAS_BACKEND_VULKAN)
    /// The Vulkan backend with a VkRenderPass ("VulkanRenderPass") or with dynamic rendering ("Vulkan").
    std::unique_ptr<BackendHarness> CreateVulkanHarness(bool useRenderPass);
#endif
} // namespace Carbon
