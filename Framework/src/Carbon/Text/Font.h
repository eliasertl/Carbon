#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace Carbon
{
    /// A font family registered with a context: an upright face and optionally an italic one. Opaque; obtained
    /// from AddFontFromMemory, AddFontFromFile or GetDefaultFont and owned by the context.
    struct Font;

    /// Font weights on the usual 100-900 scale. Variable fonts (such as the default, Public Sans) accept any value
    /// in between; cast a number to use one, e.g. FontWeight(450).
    enum class FontWeight : uint16_t
    {
        Thin = 100,
        ExtraLight = 200,
        Light = 300,
        Regular = 400,
        Medium = 500,
        Semibold = 600,
        Bold = 700,
        Heavy = 800,
        Black = 900
    };

    /// Options for registering a font.
    struct FontDescription
    {
        /// Name used in log messages.
        std::string_view Name = {};
        /// Font file data of the matching italic face. Without it, italic text uses the upright face.
        std::span<const uint8_t> ItalicData = {};
    };

    /// Registers a font (TrueType or OpenType data). The data is copied. Returns null if it cannot be parsed.
    /// Fonts added by the host are also used as fallbacks, in the order they were added, for characters the
    /// requested font lacks.
    Font* AddFontFromMemory(std::span<const uint8_t> data, const FontDescription& description = {});

    /// Reads a font file and registers it. Returns null if the file cannot be read or parsed.
    Font* AddFontFromFile(const std::filesystem::path& path, const FontDescription& description = {});

    /// The embedded default font, Public Sans.
    Font* GetDefaultFont();

    /// The embedded monospaced font, JetBrains Mono (variable weight 100-800, upright and italic). Use it for code
    /// and for numbers that must line up, through TextOptions::Font or PushFont.
    Font* GetMonospacedFont();
} // namespace Carbon
