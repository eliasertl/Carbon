#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Carbon/Carbon.h"
#include "Support/BackendHarness.h"

namespace Carbon
{
    /// A fixed interface that exercises everything a backend draws: squircle fills, strokes and shadows,
    /// circles and lines, text in several weights and sizes, icons, an image with rounded corners from a host
    /// texture, clipping, and the core controls. Used by the smoke and pixel-comparison tests.
    class TestScene
    {
    public:
        /// Size of the scene in points.
        static constexpr float Width = 360.0f;
        static constexpr float Height = 260.0f;

        /// The texels of the scene's image: an 8 x 8 RGBA8 pattern of colored quadrants with a diagonal.
        static std::vector<uint8_t> GetImageTexels();
        static constexpr uint32_t ImageSize = 8;

        /// Builds one frame of the scene. Call between NewFrame and EndFrame.
        void Build(TextureID image);

    private:
        std::string m_Name = "Carbon";
        float m_Value = 0.65f;
        bool m_IsOn = true;
        bool m_IsChecked = true;
    };

    /// Renders the scene with `harness`, which must have its device created, into a new context in `theme` at
    /// `contentScale`. The layout and animations settle over several frames first; only the last one is read back.
    /// Every warning, error or failed check during that is added to `problems`.
    RenderedImage RenderTestScene(BackendHarness& harness, bool isDark, float contentScale,
                                  std::vector<std::string>& problems);
} // namespace Carbon
