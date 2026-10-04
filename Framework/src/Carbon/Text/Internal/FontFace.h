#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

// Forward declarations matching FreeType's and HarfBuzz's own, so this header needs neither library.
typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_* FT_Face;
typedef struct hb_face_t hb_face_t;
typedef struct hb_font_t hb_font_t;

namespace Carbon::Internal
{
    /// A rasterized glyph. The pixels belong to the face and stay valid until its next Rasterize call.
    struct GlyphBitmap
    {
        const uint8_t* Pixels = nullptr;
        uint32_t Width = 0;
        uint32_t Height = 0;
        int32_t Pitch = 0;
        /// Offset in pixels from the pen position to the bitmap's left edge.
        int32_t Left = 0;
        /// Offset in pixels from the baseline up to the bitmap's top edge.
        int32_t Top = 0;
    };

    /// One font file: a FreeType face for rasterizing and HarfBuzz fonts (one per weight) for shaping.
    class FontFace
    {
    public:
        /// Parses a font. `data` must outlive the face unless `ownedData` holds the bytes. Returns null when
        /// FreeType or HarfBuzz reject the data.
        static std::unique_ptr<FontFace> Create(FT_Library library, std::span<const uint8_t> data,
                                                std::vector<uint8_t> ownedData = {});
        ~FontFace();

        FontFace(const FontFace&) = delete;
        FontFace& operator=(const FontFace&) = delete;

        /// The glyph for a code point, or 0 when the font has none.
        uint32_t GetGlyphIndex(char32_t codepoint) const;

        /// The weight actually used for a requested one: clamped to the font's weight axis, or the font's fixed
        /// weight when it has no axis. Caches key on this, so static fonts are not cached once per weight.
        uint16_t ResolveWeight(uint16_t weight) const;

        /// The HarfBuzz font for a resolved weight, scaled to font units.
        hb_font_t* GetShapingFont(uint16_t resolvedWeight);

        /// Renders a glyph at `pixelSize` pixels per em, shifted right by `subpixelX` (0..1) pixels.
        bool Rasterize(uint32_t glyphIndex, uint16_t resolvedWeight, float pixelSize, float subpixelX,
                       GlyphBitmap& bitmap);

        float GetUnitsPerEm() const { return m_UnitsPerEm; }
        /// Vertical metrics as fractions of the em.
        float GetAscent() const { return m_Ascent; }
        float GetDescent() const { return m_Descent; }
        float GetLineGap() const { return m_LineGap; }
        float GetCapHeight() const { return m_CapHeight; }
        bool HasWeightAxis() const { return m_WeightAxis >= 0; }

    private:
        FontFace() = default;

        void ApplyWeight(uint16_t resolvedWeight);

    private:
        struct ShapingFont
        {
            uint16_t Weight = 0;
            hb_font_t* Font = nullptr;
        };

        std::vector<uint8_t> m_OwnedData;
        FT_Face m_Face = nullptr;
        hb_face_t* m_ShapingFace = nullptr;
        std::vector<ShapingFont> m_ShapingFonts;

        /// Design coordinates of every variation axis (16.16 fixed point); the weight entry is overwritten.
        std::vector<long> m_AxisCoordinates;
        int32_t m_WeightAxis = -1;
        uint16_t m_MinWeight = 400;
        uint16_t m_MaxWeight = 400;
        uint16_t m_AppliedWeight = 0;
        float m_AppliedPixelSize = 0.0f;

        float m_UnitsPerEm = 1000.0f;
        float m_Ascent = 0.8f;
        float m_Descent = 0.2f;
        float m_LineGap = 0.0f;
        float m_CapHeight = 0.7f;
    };
} // namespace Carbon::Internal
