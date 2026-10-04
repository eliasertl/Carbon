#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <string>
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

    /// Test fixture that renders Carbon frames offscreen through Dawn and reads the pixels back.
    ///
    /// The tests are skipped when the machine has no WebGPU adapter (for example a CI runner without a GPU or a
    /// software rasterizer), so the suite stays green everywhere.
    class GpuTest : public ::testing::Test
    {
    protected:
        void SetUp() override;
        void TearDown() override;

        /// Runs one frame: `build` draws into the frame's draw list, then the frame is rendered into a target of
        /// `width` x `height` points at `contentScale`, cleared to `background`.
        RenderedImage RenderFrame(float width, float height, float contentScale, Color background,
                                  const std::function<void(DrawList&)>& build);

    protected:
        wgpu::Instance m_Instance;
        wgpu::Adapter m_Adapter;
        wgpu::Device m_Device;
        Context* m_Context = nullptr;
        std::vector<std::string> m_AssertMessages;
        /// WebGPU validation errors reported by the device; every test expects none.
        static std::vector<std::string> s_DeviceErrors;
    };
} // namespace Carbon
