#pragma once

#include <cstdint>
#include <string_view>

namespace Carbon
{
    /// Pixel formats of the targets a host lets Carbon render into, independent of the graphics API. Each
    /// renderer backend takes the formats of the host's pass in its init info and maps them to its own.
    enum class TextureFormat : uint8_t
    {
        /// No attachment, for example a pass without depth-stencil.
        Undefined,

        /// 8 bits per channel. Carbon writes its sRGB colors as they are and blends in gamma space.
        RGBA8Unorm,
        /// 8 bits per channel, sRGB-encoded. Carbon writes linear values and the GPU blends in linear space.
        RGBA8UnormSrgb,
        /// As RGBA8Unorm with blue first; the usual format of a window's surface.
        BGRA8Unorm,
        /// As RGBA8UnormSrgb with blue first.
        BGRA8UnormSrgb,
        /// 10 bits per color channel and 2 bits of alpha.
        RGB10A2Unorm,
        /// 16-bit floating point per channel.
        RGBA16Float,

        /// 16-bit depth.
        Depth16Unorm,
        /// 24-bit depth. Backends whose API has no exact equivalent use the closest format with at least 24 bits.
        Depth24Unorm,
        /// 24-bit depth with an 8-bit stencil.
        Depth24UnormStencil8,
        /// 32-bit floating-point depth.
        Depth32Float,
        /// 32-bit floating-point depth with an 8-bit stencil.
        Depth32FloatStencil8,
        /// 8-bit stencil without depth.
        Stencil8
    };

    /// True for the sRGB-encoded color formats. Carbon converts its colors to linear values for these.
    constexpr bool IsSrgbFormat(TextureFormat format)
    {
        return format == TextureFormat::RGBA8UnormSrgb || format == TextureFormat::BGRA8UnormSrgb;
    }

    /// True for the formats of a depth-stencil attachment.
    constexpr bool IsDepthStencilFormat(TextureFormat format)
    {
        return format >= TextureFormat::Depth16Unorm && format <= TextureFormat::Stencil8;
    }

    /// True for the color formats.
    constexpr bool IsColorFormat(TextureFormat format)
    {
        return format >= TextureFormat::RGBA8Unorm && format <= TextureFormat::RGBA16Float;
    }

    /// True for the depth-stencil formats that have a depth aspect.
    constexpr bool HasDepth(TextureFormat format)
    {
        return IsDepthStencilFormat(format) && format != TextureFormat::Stencil8;
    }

    /// True for the depth-stencil formats that have a stencil aspect.
    constexpr bool HasStencil(TextureFormat format)
    {
        return format == TextureFormat::Depth24UnormStencil8 || format == TextureFormat::Depth32FloatStencil8 ||
               format == TextureFormat::Stencil8;
    }

    /// Returns the name of a format, e.g. "BGRA8Unorm".
    std::string_view ToString(TextureFormat format);
} // namespace Carbon
