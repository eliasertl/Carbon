#include "Carbon/Assets/EmbeddedAssets.h"

#include <cstddef>

// Defined in the .cpp files that EmbedAsset.cmake generates into the build tree.
namespace Carbon::Internal
{
    extern const unsigned char g_PublicSansRomanData[];
    extern const unsigned long long g_PublicSansRomanSize;
    extern const unsigned char g_PublicSansItalicData[];
    extern const unsigned long long g_PublicSansItalicSize;
    extern const unsigned char g_JetBrainsMonoRomanData[];
    extern const unsigned long long g_JetBrainsMonoRomanSize;
    extern const unsigned char g_JetBrainsMonoItalicData[];
    extern const unsigned long long g_JetBrainsMonoItalicSize;
    extern const unsigned char g_PhosphorRegularData[];
    extern const unsigned long long g_PhosphorRegularSize;
    extern const unsigned char g_PhosphorBoldData[];
    extern const unsigned long long g_PhosphorBoldSize;
    extern const unsigned char g_PhosphorFillData[];
    extern const unsigned long long g_PhosphorFillSize;
    extern const unsigned char g_CarbonShaderData[];
    extern const unsigned long long g_CarbonShaderSize;
} // namespace Carbon::Internal

namespace Carbon
{
    std::span<const uint8_t> GetEmbeddedFont(EmbeddedFont font)
    {
        switch (font)
        {
            case EmbeddedFont::PublicSansRoman:
                return {Internal::g_PublicSansRomanData, static_cast<size_t>(Internal::g_PublicSansRomanSize)};
            case EmbeddedFont::PublicSansItalic:
                return {Internal::g_PublicSansItalicData, static_cast<size_t>(Internal::g_PublicSansItalicSize)};
            case EmbeddedFont::JetBrainsMonoRoman:
                return {Internal::g_JetBrainsMonoRomanData, static_cast<size_t>(Internal::g_JetBrainsMonoRomanSize)};
            case EmbeddedFont::JetBrainsMonoItalic:
                return {Internal::g_JetBrainsMonoItalicData, static_cast<size_t>(Internal::g_JetBrainsMonoItalicSize)};
            case EmbeddedFont::PhosphorRegular:
                return {Internal::g_PhosphorRegularData, static_cast<size_t>(Internal::g_PhosphorRegularSize)};
            case EmbeddedFont::PhosphorBold:
                return {Internal::g_PhosphorBoldData, static_cast<size_t>(Internal::g_PhosphorBoldSize)};
            case EmbeddedFont::PhosphorFill:
                return {Internal::g_PhosphorFillData, static_cast<size_t>(Internal::g_PhosphorFillSize)};
            case EmbeddedFont::Count:
                break;
        }
        return {};
    }

    std::string_view GetEmbeddedShader()
    {
        return {reinterpret_cast<const char*>(Internal::g_CarbonShaderData),
                static_cast<size_t>(Internal::g_CarbonShaderSize)};
    }
} // namespace Carbon
