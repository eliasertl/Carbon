#pragma once

#include <cstdint>

#include "ExampleArguments.h"

namespace Example
{
    /// Saves a rendered frame (8-bit RGBA, rows from the top, `bytesPerRow` apart) as the PNG named by
    /// --screenshot. Only `area` (x, y, width, height in points) is kept when given, else the --crop area, grown by
    /// --extend and kept inside the image. Prints what was saved; returns false, with the reason on stderr, when
    /// the file could not be written.
    bool SaveScreenshot(const Arguments& arguments, const uint8_t* pixels, uint32_t width, uint32_t height,
                        uint32_t bytesPerRow, float contentScale, const float* area = nullptr);
} // namespace Example
