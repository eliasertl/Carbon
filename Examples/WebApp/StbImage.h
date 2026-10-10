#pragma once

#include <string_view>

namespace WebApp
{
    /// Decodes a PNG into RGBA8 pixels, rows from the top; null when it cannot. Free the pixels with FreeImage.
    unsigned char* DecodeImage(std::string_view data, int& width, int& height);
    void FreeImage(unsigned char* pixels);
} // namespace WebApp
