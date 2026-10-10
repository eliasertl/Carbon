#pragma once

#include <cstdint>
#include <vector>

namespace Carbon::TestFonts
{
    /// Code points of the test fonts. The color fonts map ColorCodepoint to their color glyph and PlainCodepoint
    /// to a glyph without color; ZwjCodepoint is a zero-width joiner, mapped to an empty glyph.
    inline constexpr char32_t ColorCodepoint = 0x1F600;       // 😀
    inline constexpr char32_t SecondColorCodepoint = 0x1F44D; // 👍
    inline constexpr char32_t PlainCodepoint = U'X';
    inline constexpr char32_t ZwjCodepoint = 0x200D;

    /// The colors of the test fonts' color glyphs. COLR: an outer square of one em with an inner square on top in
    /// its top-left quarter. CBDT: the top half in the outer color, the bottom half in the inner one. Neither is
    /// symmetric, so an image upside down or mirrored shows.
    inline constexpr uint32_t OuterColor = 0xFF2020; // red, as 0xRRGGBB
    inline constexpr uint32_t InnerColor = 0x2040FF; // blue

    /// Units per em of the test fonts; their glyphs are squares of the whole em, from the baseline up.
    inline constexpr uint16_t UnitsPerEm = 1000;

    /// A TrueType font with a COLR (version 0) and CPAL table: the color glyph is an OuterColor square of one em
    /// with an InnerColor square of 0.4 em in its top-left quarter. The plain glyph is a square without color.
    std::vector<uint8_t> MakeColrFont();

    /// A font of bitmaps only, like Noto Color Emoji: a CBDT and CBLC table with one strike of `ppem` pixels per
    /// em, whose color glyphs are PNG images of a square, OuterColor above InnerColor. It has no plain glyph.
    std::vector<uint8_t> MakeCbdtFont(uint8_t ppem = 64);

    /// A font of bitmaps only in Apple's format: an sbix table with one strike of `ppem` pixels per em, whose color
    /// glyphs are the PNG images of MakeCbdtFont, standing on the baseline. It has no plain glyph.
    std::vector<uint8_t> MakeSbixFont(uint16_t ppem = 64);

    /// A TrueType font without color: squares for ColorCodepoint and PlainCodepoint and an empty joiner, so that
    /// it competes with the color fonts in fallback selection, as text fonts with a few emoji-like symbols do.
    std::vector<uint8_t> MakePlainFont();
} // namespace Carbon::TestFonts
