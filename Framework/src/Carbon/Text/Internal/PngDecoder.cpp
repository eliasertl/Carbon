#include "Carbon/Text/Internal/PngDecoder.h"

#include <climits>
#include <cstring>

// stb_image is compiled into this file alone, with internal linkage, so that it never clashes with a copy the host
// application links itself. Only its PNG decoder is needed.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include <stb_image.h>

namespace Carbon::Internal
{
    bool DecodePng(std::span<const uint8_t> data, DecodedImage& image)
    {
        image = DecodedImage();
        if (data.empty() || data.size() > static_cast<size_t>(INT_MAX))
            return false;

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels,
                                                STBI_rgb_alpha);
        if (pixels == nullptr)
            return false;
        image.Width = static_cast<uint32_t>(width);
        image.Height = static_cast<uint32_t>(height);
        image.Pixels.resize(static_cast<size_t>(image.Width) * image.Height * 4);
        std::memcpy(image.Pixels.data(), pixels, image.Pixels.size());
        stbi_image_free(pixels);
        return true;
    }
} // namespace Carbon::Internal
