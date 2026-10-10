// stb_image is third-party code: this file is compiled without Carbon's warning flags.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include <stb_image.h>

#include "StbImage.h"

namespace WebApp
{
    unsigned char* DecodeImage(std::string_view data, int& width, int& height)
    {
        int channels = 0;
        return stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data.data()), static_cast<int>(data.size()),
                                     &width, &height, &channels, STBI_rgb_alpha);
    }

    void FreeImage(unsigned char* pixels)
    {
        stbi_image_free(pixels);
    }
} // namespace WebApp
