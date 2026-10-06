#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <Carbon/Carbon.h>

struct GLFWwindow;

namespace Example
{
    /// One graphics API as the examples' host uses it: the device, the window's swapchain (or an offscreen target
    /// for screenshots), clearing, Carbon's backend for that API, and reading a frame back. Host and App drive it;
    /// every example except the minimal ones is written once against it and built once per backend, each build
    /// linking the implementation of one API (Examples/Common/Devices).
    ///
    /// Nothing like this exists inside Carbon: it is the part an application already has.
    class GraphicsDevice
    {
    public:
        virtual ~GraphicsDevice() = default;

        /// The backend's name, as in the executables' names ("WebGPU", "DX11").
        virtual std::string_view GetName() const = 0;

        /// Sets the GLFW window hints for the window this device renders into (client API, context version).
        virtual void SetWindowHints() const = 0;

        /// True when the device needs a window also in screenshot mode, where it stays hidden (OpenGL contexts,
        /// Direct3D 9 devices).
        virtual bool NeedsWindowOffscreen() const { return false; }

        /// Creates the device. With `isOffscreen`, frames go into an offscreen target of `width` x `height` pixels,
        /// RGBA8, which ReadPixels reads; `window` is then null or hidden. Prints why it failed to stderr.
        virtual bool Create(GLFWwindow* window, bool isOffscreen, uint32_t width, uint32_t height) = 0;

        /// Installs Carbon's backend for this API into the current context (`<Name>Init`).
        virtual bool InitCarbon() = 0;

        /// Removes it again (`<Name>Shutdown`), while the device still exists.
        virtual void ShutdownCarbon() = 0;

        /// Prepares a frame of `width` x `height` pixels: resizes the swapchain when needed and acquires the image.
        /// Returns false when there is nothing to render into now (device lost); the host waits and tries again.
        virtual bool BeginFrame(uint32_t width, uint32_t height) = 0;

        /// Clears the frame to `background` and draws Carbon's interface into it (`<Name>Render`).
        virtual void Render(Carbon::Color background) = 0;

        /// Submits the frame and presents it, or finishes it in offscreen mode.
        virtual void EndFrame() = 0;

        /// Reads the last offscreen frame back: RGBA8, rows from the top, `width * 4` bytes each.
        virtual bool ReadPixels(std::vector<uint8_t>& pixels) = 0;

        /// Creates an RGBA8 texture from `texels` (rows from the top) that lives as long as the device, and returns
        /// its ID: the native handle given to MakeTextureID, as an application would do.
        virtual Carbon::TextureID CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> texels) = 0;
    };

    /// Creates the device of the backend this executable was built for. Each library in Examples/Common/Devices
    /// defines it once.
    std::unique_ptr<GraphicsDevice> CreateGraphicsDevice();
} // namespace Example
