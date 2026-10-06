#pragma once

#include <cstdint>
#include <filesystem>

#include "Support/BackendHarness.h"

namespace Carbon
{
    /// How far two images are apart.
    struct ImageDifference
    {
        /// Pixels whose largest channel difference exceeds the tolerance.
        uint64_t MismatchedPixels = 0;
        uint64_t TotalPixels = 0;
        /// The largest channel difference over all pixels, 0-255.
        uint32_t LargestDifference = 0;
        bool SizesMatch = true;

        /// Mismatched pixels as a percentage of all pixels.
        double GetMismatchPercentage() const
        {
            return TotalPixels == 0 ? 0.0 : 100.0 * static_cast<double>(MismatchedPixels) / TotalPixels;
        }
    };

    /// Compares two RGBA8 images; a pixel mismatches when a channel differs by more than `channelTolerance`.
    ImageDifference CompareImages(const RenderedImage& actual, const RenderedImage& reference,
                                  uint32_t channelTolerance);

    /// An image that shows where two images differ: mismatched pixels in red, the rest a faded copy of the
    /// reference.
    RenderedImage MakeDifferenceImage(const RenderedImage& actual, const RenderedImage& reference,
                                      uint32_t channelTolerance);

    /// Writes an image as PNG. Returns false when the file could not be written.
    bool WritePng(const std::filesystem::path& path, const RenderedImage& image);

    /// The folder where tests write images that explain a failure: <build>/Tests/<name>, created on demand.
    std::filesystem::path GetTestOutputDirectory(std::string_view name);
} // namespace Carbon
