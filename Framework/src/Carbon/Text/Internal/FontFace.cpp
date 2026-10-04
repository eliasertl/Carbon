#include "Carbon/Text/Internal/FontFace.h"

#include <algorithm>
#include <cmath>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

#include <hb.h>

namespace Carbon::Internal
{
    namespace
    {
        constexpr FT_ULong WeightTag = FT_MAKE_TAG('w', 'g', 'h', 't');
    } // namespace

    std::unique_ptr<FontFace> FontFace::Create(FT_Library library, std::span<const uint8_t> data,
                                               std::vector<uint8_t> ownedData)
    {
        std::unique_ptr<FontFace> face(new FontFace());
        face->m_OwnedData = std::move(ownedData);
        if (!face->m_OwnedData.empty())
            data = face->m_OwnedData;
        if (data.empty())
            return nullptr;

        if (FT_New_Memory_Face(library, data.data(), static_cast<FT_Long>(data.size()), 0, &face->m_Face) != 0)
            return nullptr;
        if (!FT_IS_SCALABLE(face->m_Face))
            return nullptr;

        hb_blob_t* blob =
            hb_blob_create(reinterpret_cast<const char*>(data.data()), static_cast<unsigned int>(data.size()),
                           HB_MEMORY_MODE_READONLY, nullptr, nullptr);
        face->m_ShapingFace = hb_face_create(blob, 0);
        hb_blob_destroy(blob);
        if (hb_face_get_glyph_count(face->m_ShapingFace) == 0)
            return nullptr;

        const FT_Face ft = face->m_Face;
        face->m_UnitsPerEm = static_cast<float>(ft->units_per_EM);
        face->m_Ascent = static_cast<float>(ft->ascender) / face->m_UnitsPerEm;
        face->m_Descent = -static_cast<float>(ft->descender) / face->m_UnitsPerEm;
        face->m_LineGap =
            std::max(0.0f, static_cast<float>(ft->height - ft->ascender + ft->descender) / face->m_UnitsPerEm);

        const TT_OS2* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(ft, FT_SFNT_OS2));
        if (os2 != nullptr && os2->version >= 2 && os2->sCapHeight > 0)
            face->m_CapHeight = static_cast<float>(os2->sCapHeight) / face->m_UnitsPerEm;
        if (os2 != nullptr && os2->usWeightClass >= 1 && os2->usWeightClass <= 1000)
        {
            face->m_MinWeight = os2->usWeightClass;
            face->m_MaxWeight = os2->usWeightClass;
        }

        FT_MM_Var* variations = nullptr;
        if (FT_HAS_MULTIPLE_MASTERS(ft) && FT_Get_MM_Var(ft, &variations) == 0)
        {
            for (FT_UInt axis = 0; axis < variations->num_axis; axis++)
            {
                face->m_AxisCoordinates.push_back(variations->axis[axis].def);
                if (variations->axis[axis].tag == WeightTag)
                {
                    face->m_WeightAxis = static_cast<int32_t>(axis);
                    face->m_MinWeight = static_cast<uint16_t>(variations->axis[axis].minimum >> 16);
                    face->m_MaxWeight = static_cast<uint16_t>(variations->axis[axis].maximum >> 16);
                }
            }
            FT_Done_MM_Var(library, variations);
        }

        return face;
    }

    FontFace::~FontFace()
    {
        for (const ShapingFont& shapingFont : m_ShapingFonts)
            hb_font_destroy(shapingFont.Font);
        if (m_ShapingFace != nullptr)
            hb_face_destroy(m_ShapingFace);
        if (m_Face != nullptr)
            FT_Done_Face(m_Face);
    }

    uint32_t FontFace::GetGlyphIndex(char32_t codepoint) const
    {
        return FT_Get_Char_Index(m_Face, static_cast<FT_ULong>(codepoint));
    }

    uint16_t FontFace::ResolveWeight(uint16_t weight) const
    {
        return std::clamp(weight, m_MinWeight, m_MaxWeight);
    }

    hb_font_t* FontFace::GetShapingFont(uint16_t resolvedWeight)
    {
        for (const ShapingFont& shapingFont : m_ShapingFonts)
        {
            if (shapingFont.Weight == resolvedWeight)
                return shapingFont.Font;
        }

        // The default scale of a HarfBuzz font is the face's units per em, so shaping results are in font units
        // and independent of the text size.
        hb_font_t* font = hb_font_create(m_ShapingFace);
        if (HasWeightAxis())
        {
            hb_variation_t variation;
            variation.tag = HB_TAG('w', 'g', 'h', 't');
            variation.value = static_cast<float>(resolvedWeight);
            hb_font_set_variations(font, &variation, 1);
        }
        m_ShapingFonts.push_back(ShapingFont{resolvedWeight, font});
        return font;
    }

    bool FontFace::Rasterize(uint32_t glyphIndex, uint16_t resolvedWeight, float pixelSize, float subpixelX,
                             GlyphBitmap& bitmap)
    {
        bitmap = GlyphBitmap();
        ApplyWeight(resolvedWeight);

        if (pixelSize != m_AppliedPixelSize)
        {
            // 26.6 fixed point; at 72 dpi one point is one pixel.
            if (FT_Set_Char_Size(m_Face, 0, static_cast<FT_F26Dot6>(std::lround(pixelSize * 64.0f)), 72, 72) != 0)
                return false;
            m_AppliedPixelSize = pixelSize;
        }

        // No hinting: glyph shapes and advances stay faithful to the design at every size, like on macOS.
        if (FT_Load_Glyph(m_Face, glyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0)
            return false;
        const FT_GlyphSlot slot = m_Face->glyph;
        if (slot->format != FT_GLYPH_FORMAT_OUTLINE)
            return false;

        if (subpixelX != 0.0f)
            FT_Outline_Translate(&slot->outline, static_cast<FT_Pos>(std::lround(subpixelX * 64.0f)), 0);
        if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0)
            return false;

        bitmap.Pixels = slot->bitmap.buffer;
        bitmap.Width = slot->bitmap.width;
        bitmap.Height = slot->bitmap.rows;
        bitmap.Pitch = slot->bitmap.pitch;
        bitmap.Left = slot->bitmap_left;
        bitmap.Top = slot->bitmap_top;
        return true;
    }

    void FontFace::ApplyWeight(uint16_t resolvedWeight)
    {
        if (!HasWeightAxis() || resolvedWeight == m_AppliedWeight)
            return;
        m_AxisCoordinates[static_cast<size_t>(m_WeightAxis)] = static_cast<long>(resolvedWeight) << 16;
        FT_Set_Var_Design_Coordinates(m_Face, static_cast<FT_UInt>(m_AxisCoordinates.size()),
                                      reinterpret_cast<FT_Fixed*>(m_AxisCoordinates.data()));
        m_AppliedWeight = resolvedWeight;
    }
} // namespace Carbon::Internal
