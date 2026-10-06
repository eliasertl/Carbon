#include "Support/ImageCompare.h"

#include <algorithm>
#include <cstdlib>
#include <system_error>

#include <stb_image_write.h>

namespace Carbon
{
    namespace
    {
        uint32_t GetLargestChannelDifference(const uint8_t* a, const uint8_t* b)
        {
            uint32_t largest = 0;
            for (int channel = 0; channel < 4; channel++)
                largest =
                    std::max<uint32_t>(largest, static_cast<uint32_t>(std::abs(int(a[channel]) - int(b[channel]))));
            return largest;
        }
    } // namespace

    ImageDifference CompareImages(const RenderedImage& actual, const RenderedImage& reference,
                                  uint32_t channelTolerance)
    {
        ImageDifference difference;
        if (actual.Width != reference.Width || actual.Height != reference.Height ||
            actual.Pixels.size() != reference.Pixels.size())
        {
            difference.SizesMatch = false;
            return difference;
        }

        difference.TotalPixels = static_cast<uint64_t>(actual.Width) * actual.Height;
        for (size_t i = 0; i < actual.Pixels.size(); i += 4)
        {
            const uint32_t largest = GetLargestChannelDifference(&actual.Pixels[i], &reference.Pixels[i]);
            difference.LargestDifference = std::max(difference.LargestDifference, largest);
            if (largest > channelTolerance)
                difference.MismatchedPixels++;
        }
        return difference;
    }

    RenderedImage MakeDifferenceImage(const RenderedImage& actual, const RenderedImage& reference,
                                      uint32_t channelTolerance)
    {
        RenderedImage image;
        image.Width = reference.Width;
        image.Height = reference.Height;
        image.Pixels.resize(reference.Pixels.size());
        const bool sizesMatch = actual.Pixels.size() == reference.Pixels.size();
        for (size_t i = 0; i < reference.Pixels.size(); i += 4)
        {
            const bool mismatches =
                !sizesMatch || GetLargestChannelDifference(&actual.Pixels[i], &reference.Pixels[i]) > channelTolerance;
            for (int channel = 0; channel < 3; channel++)
            {
                const uint8_t faded = static_cast<uint8_t>(reference.Pixels[i + channel] / 4 + 191);
                image.Pixels[i + channel] = mismatches ? (channel == 0 ? 255 : 0) : faded;
            }
            image.Pixels[i + 3] = 255;
        }
        return image;
    }

    bool WritePng(const std::filesystem::path& path, const RenderedImage& image)
    {
        if (image.Width == 0 || image.Height == 0)
            return false;
        return stbi_write_png(path.string().c_str(), static_cast<int>(image.Width), static_cast<int>(image.Height), 4,
                              image.Pixels.data(), static_cast<int>(image.Width * 4)) != 0;
    }

    bool ShouldWriteAllImages()
    {
#if defined(_MSC_VER)
        char* value = nullptr;
        size_t length = 0;
        const bool isSet = _dupenv_s(&value, &length, "CARBON_TESTS_WRITE_IMAGES") == 0 && value != nullptr;
        std::free(value);
        return isSet;
#else
        return std::getenv("CARBON_TESTS_WRITE_IMAGES") != nullptr;
#endif
    }

    std::filesystem::path GetTestOutputDirectory(std::string_view name)
    {
        const std::filesystem::path directory = std::filesystem::path(CARBON_TESTS_OUTPUT_DIR) / name;
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        return directory;
    }
} // namespace Carbon
