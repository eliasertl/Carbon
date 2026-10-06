#include "Screenshot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <stb_image_write.h>

namespace Example
{
    bool SaveScreenshot(const Arguments& arguments, const uint8_t* pixels, uint32_t width, uint32_t height,
                        uint32_t bytesPerRow, float contentScale, const float* area)
    {
        // The area to keep: what the example asked for, or --crop, grown by --extend and kept inside the image.
        if (area == nullptr)
            area = arguments.Crop;
        uint32_t left = 0;
        uint32_t top = 0;
        uint32_t right = width;
        uint32_t bottom = height;
        if (area[2] > 0.0f && area[3] > 0.0f)
        {
            const float* extend = arguments.Extend;
            const auto toPixels = [contentScale](float points, uint32_t limit)
            {
                return static_cast<uint32_t>(
                    std::clamp(std::lround(points * contentScale), 0L, static_cast<long>(limit)));
            };
            left = toPixels(area[0] - extend[0], width);
            top = toPixels(area[1] - extend[1], height);
            right = std::max(toPixels(area[0] + area[2] + extend[2], width), left + 1);
            bottom = std::max(toPixels(area[1] + area[3] + extend[3], height), top + 1);
        }

        const uint8_t* first = pixels + static_cast<size_t>(top) * bytesPerRow + static_cast<size_t>(left) * 4;
        const int written = stbi_write_png(arguments.ScreenshotPath.c_str(), static_cast<int>(right - left),
                                           static_cast<int>(bottom - top), 4, first, static_cast<int>(bytesPerRow));
        if (written == 0)
        {
            std::fprintf(stderr, "Could not write '%s'\n", arguments.ScreenshotPath.c_str());
            return false;
        }
        std::printf("Saved %s (%u x %u pixels, scale %.2f)\n", arguments.ScreenshotPath.c_str(), right - left,
                    bottom - top, static_cast<double>(contentScale));
        return true;
    }
} // namespace Example
