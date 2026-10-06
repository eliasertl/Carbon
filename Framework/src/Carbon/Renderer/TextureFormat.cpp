#include "Carbon/Renderer/TextureFormat.h"

namespace Carbon
{
    std::string_view ToString(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::Undefined:
                return "Undefined";
            case TextureFormat::RGBA8Unorm:
                return "RGBA8Unorm";
            case TextureFormat::RGBA8UnormSrgb:
                return "RGBA8UnormSrgb";
            case TextureFormat::BGRA8Unorm:
                return "BGRA8Unorm";
            case TextureFormat::BGRA8UnormSrgb:
                return "BGRA8UnormSrgb";
            case TextureFormat::RGB10A2Unorm:
                return "RGB10A2Unorm";
            case TextureFormat::RGBA16Float:
                return "RGBA16Float";
            case TextureFormat::Depth16Unorm:
                return "Depth16Unorm";
            case TextureFormat::Depth24Unorm:
                return "Depth24Unorm";
            case TextureFormat::Depth24UnormStencil8:
                return "Depth24UnormStencil8";
            case TextureFormat::Depth32Float:
                return "Depth32Float";
            case TextureFormat::Depth32FloatStencil8:
                return "Depth32FloatStencil8";
            case TextureFormat::Stencil8:
                return "Stencil8";
        }
        return "Unknown";
    }
} // namespace Carbon
