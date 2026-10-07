#include "Carbon/Text/Internal/FontFace.h"

#include <algorithm>
#include <cmath>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

#include <hb-ot.h>
#include <hb.h>
#if defined(CARBON_HAS_HARFBUZZ_RASTER)
#include <hb-raster.h>
#endif

namespace Carbon::Internal
{
    namespace
    {
        constexpr FT_ULong WeightTag = FT_MAKE_TAG('w', 'g', 'h', 't');

        // Calls `visit(source, weight)` for the source pixels that make up target pixel `index` when `sourceSize`
        // pixels are scaled to `targetSize`: each with the share of the target pixel it covers. Shrinking averages
        // the pixels under the target pixel; enlarging repeats the nearest one.
        template <typename Visit>
        void ForEachSourcePixel(uint32_t index, uint32_t sourceSize, uint32_t targetSize, Visit visit)
        {
            const double scale = static_cast<double>(sourceSize) / static_cast<double>(targetSize);
            const double start = static_cast<double>(index) * scale;
            const double end = static_cast<double>(index + 1) * scale;
            const uint32_t first = static_cast<uint32_t>(std::floor(start));
            const uint32_t last = std::min(static_cast<uint32_t>(std::ceil(end)), sourceSize);
            for (uint32_t source = first; source < last; source++)
            {
                const double overlap =
                    std::min(end, static_cast<double>(source) + 1.0) - std::max(start, static_cast<double>(source));
                if (overlap > 0.0)
                    visit(source, static_cast<float>(overlap / (end - start)));
            }
        }

        // Scales a straight-alpha RGBA image to `width` x `height` and premultiplies it: averaging premultiplied
        // colors keeps transparent pixels from darkening the edges.
        void ScaleImage(const DecodedImage& image, uint32_t width, uint32_t height, std::vector<uint8_t>& pixels)
        {
            // Rows first, into floats, then columns.
            std::vector<float> rows(static_cast<size_t>(width) * image.Height * 4, 0.0f);
            for (uint32_t y = 0; y < image.Height; y++)
            {
                const uint8_t* source = image.Pixels.data() + static_cast<size_t>(y) * image.Width * 4;
                float* target = rows.data() + static_cast<size_t>(y) * width * 4;
                for (uint32_t x = 0; x < width; x++)
                {
                    ForEachSourcePixel(x, image.Width, width,
                                       [&](uint32_t sourceX, float weight)
                                       {
                                           const uint8_t* pixel = source + static_cast<size_t>(sourceX) * 4;
                                           const float alpha = static_cast<float>(pixel[3]) / 255.0f * weight;
                                           for (int channel = 0; channel < 3; channel++)
                                               target[x * 4 + channel] += static_cast<float>(pixel[channel]) * alpha;
                                           target[x * 4 + 3] += alpha * 255.0f;
                                       });
                }
            }

            pixels.assign(static_cast<size_t>(width) * height * 4, 0);
            for (uint32_t y = 0; y < height; y++)
            {
                for (uint32_t x = 0; x < width; x++)
                {
                    float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                    ForEachSourcePixel(y, image.Height, height,
                                       [&](uint32_t sourceY, float weight)
                                       {
                                           const float* pixel =
                                               rows.data() + (static_cast<size_t>(sourceY) * width + x) * 4;
                                           for (int channel = 0; channel < 4; channel++)
                                               sum[channel] += pixel[channel] * weight;
                                       });
                    uint8_t* target = pixels.data() + (static_cast<size_t>(y) * width + x) * 4;
                    for (int channel = 0; channel < 4; channel++)
                        target[channel] = static_cast<uint8_t>(std::clamp(std::lround(sum[channel]), 0L, 255L));
                }
            }
        }

        // True when COLR has a color version of the glyph: a paint graph (version 1) or layers (version 0).
        bool HasColorPaint(hb_face_t* face, uint32_t glyphIndex)
        {
            return hb_ot_color_glyph_has_paint(face, glyphIndex) ||
                   hb_ot_color_glyph_get_layers(face, glyphIndex, 0, nullptr, nullptr) > 0;
        }
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

        hb_blob_t* blob =
            hb_blob_create(reinterpret_cast<const char*>(data.data()), static_cast<unsigned int>(data.size()),
                           HB_MEMORY_MODE_READONLY, nullptr, nullptr);
        face->m_ShapingFace = hb_face_create(blob, 0);
        hb_blob_destroy(blob);
        if (hb_face_get_glyph_count(face->m_ShapingFace) == 0)
            return nullptr;

        // Color glyphs: COLR needs HarfBuzz's raster library to be painted; PNG images Carbon decodes itself.
#if defined(CARBON_HAS_HARFBUZZ_RASTER)
        face->m_HasColorPaint =
            hb_ot_color_has_layers(face->m_ShapingFace) || hb_ot_color_has_paint(face->m_ShapingFace);
#endif
        face->m_HasColorImages = hb_ot_color_has_png(face->m_ShapingFace);
        // A font of bitmaps only (a CBDT emoji font) is usable for its color images.
        face->m_HasOutlines = FT_IS_SCALABLE(face->m_Face);
        if (!face->m_HasOutlines && !face->m_HasColorImages)
            return nullptr;

        const FT_Face ft = face->m_Face;
        if (face->m_HasOutlines)
        {
            face->m_UnitsPerEm = static_cast<float>(ft->units_per_EM);
            face->m_Ascent = static_cast<float>(ft->ascender) / face->m_UnitsPerEm;
            face->m_Descent = -static_cast<float>(ft->descender) / face->m_UnitsPerEm;
            face->m_LineGap =
                std::max(0.0f, static_cast<float>(ft->height - ft->ascender + ft->descender) / face->m_UnitsPerEm);
        }
        else
        {
            // FreeType reports no metrics in font units for bitmap fonts; HarfBuzz reads them from the tables.
            face->m_UnitsPerEm = static_cast<float>(std::max(hb_face_get_upem(face->m_ShapingFace), 1u));
            hb_font_t* font = hb_font_create(face->m_ShapingFace);
            hb_position_t ascender = 0;
            hb_position_t descender = 0;
            hb_position_t lineGap = 0;
            hb_ot_metrics_get_position(font, HB_OT_METRICS_TAG_HORIZONTAL_ASCENDER, &ascender);
            hb_ot_metrics_get_position(font, HB_OT_METRICS_TAG_HORIZONTAL_DESCENDER, &descender);
            hb_ot_metrics_get_position(font, HB_OT_METRICS_TAG_HORIZONTAL_LINE_GAP, &lineGap);
            hb_font_destroy(font);
            face->m_Ascent = static_cast<float>(ascender) / face->m_UnitsPerEm;
            face->m_Descent = -static_cast<float>(descender) / face->m_UnitsPerEm;
            face->m_LineGap = std::max(0.0f, static_cast<float>(lineGap) / face->m_UnitsPerEm);
        }

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
#if defined(CARBON_HAS_HARFBUZZ_RASTER)
        if (m_Painter != nullptr)
            hb_raster_paint_destroy(m_Painter);
#endif
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

    bool FontFace::IsColorGlyph(uint32_t glyphIndex, uint16_t resolvedWeight)
    {
        if (m_HasColorPaint && HasColorPaint(m_ShapingFace, glyphIndex))
            return true;
        if (!m_HasColorImages)
            return false;
        hb_blob_t* image = hb_ot_color_glyph_reference_png(GetShapingFont(resolvedWeight), glyphIndex);
        const bool hasImage = hb_blob_get_length(image) > 0;
        hb_blob_destroy(image);
        return hasImage;
    }

    bool FontFace::Rasterize(uint32_t glyphIndex, uint16_t resolvedWeight, float pixelSize, float subpixelX,
                             GlyphBitmap& bitmap)
    {
        bitmap = GlyphBitmap();
        if (m_HasColorPaint && PaintColorGlyph(glyphIndex, resolvedWeight, pixelSize, subpixelX, bitmap))
            return true;
        if (m_HasColorImages && DecodeColorImage(glyphIndex, resolvedWeight, pixelSize, bitmap))
            return true;
        bitmap = GlyphBitmap();
        return m_HasOutlines && RasterizeOutline(glyphIndex, resolvedWeight, pixelSize, subpixelX, bitmap);
    }

    bool FontFace::RasterizeOutline(uint32_t glyphIndex, uint16_t resolvedWeight, float pixelSize, float subpixelX,
                                    GlyphBitmap& bitmap)
    {
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

    bool FontFace::PaintColorGlyph(uint32_t glyphIndex, uint16_t resolvedWeight, float pixelSize, float subpixelX,
                                   GlyphBitmap& bitmap)
    {
#if defined(CARBON_HAS_HARFBUZZ_RASTER)
        if (!HasColorPaint(m_ShapingFace, glyphIndex))
            return false;
        if (m_Painter == nullptr)
            m_Painter = hb_raster_paint_create_or_fail();
        if (m_Painter == nullptr)
            return false;

        // The shaping font works in font units; the painter scales them to pixels.
        hb_font_t* font = GetShapingFont(resolvedWeight);
        const float scale = pixelSize / m_UnitsPerEm;
        hb_raster_paint_set_transform(m_Painter, scale, 0.0f, 0.0f, scale, subpixelX, 0.0f);
        hb_glyph_extents_t extents;
        if (!hb_font_get_glyph_extents(font, glyphIndex, &extents) ||
            !hb_raster_paint_set_glyph_extents(m_Painter, &extents) ||
            !hb_raster_paint_glyph_or_fail(m_Painter, font, glyphIndex))
        {
            hb_raster_paint_clear(m_Painter);
            return false;
        }
        hb_raster_image_t* image = hb_raster_paint_render(m_Painter);
        if (image == nullptr)
            return false;

        // HarfBuzz's image is premultiplied BGRA with its rows from the bottom (y points up in glyph space).
        hb_raster_extents_t imageExtents;
        hb_raster_image_get_extents(image, &imageExtents);
        const uint8_t* buffer = hb_raster_image_get_buffer(image);
        const uint32_t width = imageExtents.width;
        const uint32_t height = imageExtents.height;
        m_ColorPixels.resize(static_cast<size_t>(width) * height * 4);
        for (uint32_t row = 0; row < height; row++)
        {
            const uint8_t* source = buffer + static_cast<size_t>(height - 1 - row) * imageExtents.stride;
            uint8_t* target = m_ColorPixels.data() + static_cast<size_t>(row) * width * 4;
            for (uint32_t x = 0; x < width; x++)
            {
                target[x * 4 + 0] = source[x * 4 + 2];
                target[x * 4 + 1] = source[x * 4 + 1];
                target[x * 4 + 2] = source[x * 4 + 0];
                target[x * 4 + 3] = source[x * 4 + 3];
            }
        }
        hb_raster_paint_recycle_image(m_Painter, image);

        bitmap.Pixels = m_ColorPixels.data();
        bitmap.Width = width;
        bitmap.Height = height;
        bitmap.Pitch = static_cast<int32_t>(width * 4);
        bitmap.Left = imageExtents.x_origin;
        bitmap.Top = imageExtents.y_origin + static_cast<int32_t>(height);
        bitmap.Format = GlyphFormat::Color;
        return width > 0 && height > 0;
#else
        static_cast<void>(glyphIndex);
        static_cast<void>(resolvedWeight);
        static_cast<void>(pixelSize);
        static_cast<void>(subpixelX);
        static_cast<void>(bitmap);
        return false;
#endif
    }

    bool FontFace::DecodeColorImage(uint32_t glyphIndex, uint16_t resolvedWeight, float pixelSize, GlyphBitmap& bitmap)
    {
        // HarfBuzz picks the strike that suits the font's pixels per em, which the shaping font leaves unset so
        // that shaping does not depend on the size. A short-lived sub-font carries it.
        hb_font_t* font = hb_font_create_sub_font(GetShapingFont(resolvedWeight));
        const unsigned int ppem = static_cast<unsigned int>(std::ceil(pixelSize));
        hb_font_set_ppem(font, ppem, ppem);
        hb_blob_t* blob = hb_ot_color_glyph_reference_png(font, glyphIndex);
        hb_glyph_extents_t extents;
        const bool hasExtents = hb_font_get_glyph_extents(font, glyphIndex, &extents);
        hb_font_destroy(font);

        unsigned int length = 0;
        const char* data = hb_blob_get_data(blob, &length);
        const bool isDecoded =
            hasExtents && length > 0 &&
            DecodePng(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data), length), m_Image);
        hb_blob_destroy(blob);
        if (!isDecoded || m_Image.Width == 0 || m_Image.Height == 0)
            return false;

        // The extents say where the image goes, in font units; the image is scaled to fill them at this size.
        const float scale = pixelSize / m_UnitsPerEm;
        const float left = static_cast<float>(extents.x_bearing) * scale;
        const float top = static_cast<float>(extents.y_bearing) * scale;
        const float right = left + static_cast<float>(extents.width) * scale;
        const float bottom = top + static_cast<float>(extents.height) * scale; // the height is negative
        const int32_t pixelLeft = static_cast<int32_t>(std::lround(left));
        const int32_t pixelTop = static_cast<int32_t>(std::lround(top));
        const uint32_t width = static_cast<uint32_t>(std::max(1L, std::lround(right) - pixelLeft));
        const uint32_t height = static_cast<uint32_t>(std::max(1L, pixelTop - std::lround(bottom)));
        ScaleImage(m_Image, width, height, m_ColorPixels);

        bitmap.Pixels = m_ColorPixels.data();
        bitmap.Width = width;
        bitmap.Height = height;
        bitmap.Pitch = static_cast<int32_t>(width * 4);
        bitmap.Left = pixelLeft;
        bitmap.Top = pixelTop;
        bitmap.Format = GlyphFormat::Color;
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
