#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace Carbon::Internal
{
    /// A decoded image: straight-alpha RGBA, four bytes per pixel, rows from the top, without padding.
    struct DecodedImage
    {
        std::vector<uint8_t> Pixels;
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    /// Decodes PNG data, as the color glyphs of CBDT and sbix fonts are stored. Returns false when the data is not
    /// a PNG image Carbon can read.
    bool DecodePng(std::span<const uint8_t> data, DecodedImage& image);
} // namespace Carbon::Internal
