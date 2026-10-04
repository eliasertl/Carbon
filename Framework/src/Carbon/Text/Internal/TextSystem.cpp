#include "Carbon/Text/Internal/TextSystem.h"

#include <cmath>
#include <limits>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>

#include "Carbon/Assets/EmbeddedAssets.h"
#include "Carbon/Core/Hash.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Core/UTF8.h"
#include "Carbon/Draw/DrawList.h"

namespace Carbon::Internal
{
    namespace
    {
        constexpr uint16_t InvalidFace = std::numeric_limits<uint16_t>::max();
        constexpr uint32_t InitialAtlasSize = 512;
        constexpr uint32_t MaxAtlasSize = 4096;

        // Glyphs are rasterized at four horizontal sub-pixel offsets so spacing stays even at small sizes.
        constexpr int SubpixelBins = 4;

        // Icons are drawn larger than the text they sit in, so their visual size matches the capitals, and are
        // centered on the middle of the capital letters.
        constexpr float IconScale = 1.2f;

        // Shaped lines unused for this many frames are evicted; the sweep runs every EvictionInterval frames.
        constexpr uint64_t ShapedLineLifetime = 600;
        constexpr uint64_t EvictionInterval = 120;

        bool IsPrivateUse(char32_t codepoint)
        {
            return codepoint >= 0xE000 && codepoint <= 0xF8FF;
        }
    } // namespace

    size_t TextSystem::GlyphKeyHash::operator()(const GlyphKey& key) const noexcept
    {
        uint64_t hash = HashCombine(HashSeed, (static_cast<uint64_t>(key.Glyph) << 32) | key.PixelSize);
        hash = HashCombine(
            hash, (static_cast<uint64_t>(key.Face) << 32) | (static_cast<uint64_t>(key.Weight) << 8) | key.SubpixelBin);
        return static_cast<size_t>(hash);
    }

    TextSystem::TextSystem() : m_Atlas(InitialAtlasSize, InitialAtlasSize, MaxAtlasSize)
    {
        FT_Init_FreeType(&m_Library);
        m_Buffer = hb_buffer_create();

        // The default family comes first: it is the default font and the first fallback.
        AddFont(GetEmbeddedFont(EmbeddedFont::PublicSansRoman), GetEmbeddedFont(EmbeddedFont::PublicSansItalic),
                "Public Sans", false);
        m_IconRegular = AddFace(GetEmbeddedFont(EmbeddedFont::PhosphorRegular), false);
        m_IconBold = AddFace(GetEmbeddedFont(EmbeddedFont::PhosphorBold), false);
        m_IconFill = AddFace(GetEmbeddedFont(EmbeddedFont::PhosphorFill), false);
    }

    TextSystem::~TextSystem()
    {
        m_Fonts.clear();
        m_Faces.clear();
        if (m_Buffer != nullptr)
            hb_buffer_destroy(m_Buffer);
        if (m_Library != nullptr)
            FT_Done_FreeType(m_Library);
    }

    Font* TextSystem::AddFont(std::span<const uint8_t> data, std::span<const uint8_t> italicData, std::string_view name,
                              bool copyData)
    {
        const uint16_t roman = AddFace(data, copyData);
        if (roman == InvalidFace)
        {
            CB_LOG_ERROR("Text", "Font '{}' could not be parsed", name);
            return nullptr;
        }

        uint16_t italic = roman;
        if (!italicData.empty())
        {
            italic = AddFace(italicData, copyData);
            if (italic == InvalidFace)
            {
                CB_LOG_WARNING("Text", "Italic face of font '{}' could not be parsed; using the upright face", name);
                italic = roman;
            }
        }

        std::unique_ptr<Font> font = std::make_unique<Font>();
        font->Name = std::string(name);
        font->Roman = roman;
        font->Italic = italic;
        m_Fonts.push_back(std::move(font));
        // Fallback resolution depends on the set of fonts, so cached lines are stale now.
        m_ShapedLines.clear();
        return m_Fonts.back().get();
    }

    void TextSystem::BeginFrame(uint64_t frameCount, float contentScale)
    {
        m_FrameCount = frameCount;

        if (m_AtlasOverflowed || contentScale != m_ContentScale)
        {
            m_Atlas.Clear();
            m_Glyphs.clear();
            m_AtlasOverflowed = false;
        }
        m_ContentScale = contentScale;

        if (frameCount % EvictionInterval == 0)
        {
            for (auto it = m_ShapedLines.begin(); it != m_ShapedLines.end();)
            {
                if (frameCount - it->second.LastUsedFrame > ShapedLineLifetime)
                    it = m_ShapedLines.erase(it);
                else
                    ++it;
            }
        }
    }

    const ShapedLine& TextSystem::Shape(std::string_view line, const TextSpec& spec)
    {
        const Font* font = spec.Font != nullptr ? spec.Font : GetDefaultFont();
        uint64_t key = HashBytes(line);
        key = HashCombine(key, reinterpret_cast<uintptr_t>(font));
        key = HashCombine(key, (static_cast<uint64_t>(spec.Weight) << 16) | (static_cast<uint64_t>(spec.Icons) << 8) |
                                   (spec.Italic ? 1u : 0u));

        const auto found = m_ShapedLines.find(key);
        if (found != m_ShapedLines.end())
        {
            found->second.LastUsedFrame = m_FrameCount;
            return found->second;
        }

        ShapedLine shaped;
        shaped.LastUsedFrame = m_FrameCount;

        // Split the line into runs that use the same face, then shape each run.
        const uint16_t primaryFace = GetPrimaryFace(spec);
        uint16_t runFace = primaryFace;
        size_t runStart = 0;
        size_t offset = 0;
        while (offset < line.size())
        {
            const UTF8Decoded decoded = DecodeUTF8(line, offset);
            const uint16_t face = ResolveFace(decoded.Codepoint, primaryFace, runFace, spec);
            if (face != runFace)
            {
                if (offset > runStart)
                    ShapeRun(line, runStart, offset - runStart, runFace, primaryFace, spec, shaped);
                runFace = face;
                runStart = offset;
            }
            offset += decoded.Length;
        }
        if (offset > runStart)
            ShapeRun(line, runStart, offset - runStart, runFace, primaryFace, spec, shaped);

        return m_ShapedLines.emplace(key, std::move(shaped)).first->second;
    }

    FontMetrics TextSystem::GetMetrics(const TextSpec& spec)
    {
        const FontFace& face = *m_Faces[GetPrimaryFace(spec)];
        FontMetrics metrics;
        metrics.Ascent = face.GetAscent() * spec.Size;
        metrics.Descent = face.GetDescent() * spec.Size;
        metrics.CapHeight = face.GetCapHeight() * spec.Size;
        const float naturalLineHeight = (face.GetAscent() + face.GetDescent() + face.GetLineGap()) * spec.Size;
        metrics.LineHeight = spec.LineHeight > 0.0f ? spec.LineHeight : naturalLineHeight;
        metrics.Baseline = (metrics.LineHeight - (metrics.Ascent + metrics.Descent)) * 0.5f + metrics.Ascent;
        return metrics;
    }

    float TextSystem::GetGlyphX(const ShapedLine& shaped, const TextSpec& spec, size_t index)
    {
        const float tracking = spec.Tracking * static_cast<float>(index);
        if (index >= shaped.Glyphs.size())
            return shaped.Width * spec.Size + tracking;
        return shaped.Glyphs[index].X * spec.Size + tracking;
    }

    void TextSystem::BreakLines(std::string_view paragraph, const ShapedLine& shaped, const TextSpec& spec,
                                std::vector<LineRange>& lines)
    {
        const size_t count = shaped.Glyphs.size();
        const auto isSpace = [&](size_t index)
        {
            const uint32_t cluster = shaped.Glyphs[index].Cluster;
            return cluster < paragraph.size() && paragraph[cluster] == ' ';
        };
        const auto addLine = [&](size_t first, size_t end)
        {
            // Spaces at the end of a visual line do not count towards its width.
            size_t trimmed = end;
            while (trimmed > first && isSpace(trimmed - 1))
                trimmed--;
            LineRange line;
            line.First = first;
            line.End = end;
            line.Start = GetGlyphX(shaped, spec, first);
            line.Width = GetGlyphX(shaped, spec, trimmed) - line.Start;
            lines.push_back(line);
        };

        if (spec.MaxWidth <= 0.0f || !spec.Wraps || count == 0)
        {
            addLine(0, count);
            return;
        }

        // Greedy word wrap: break after the last space that fits; a word longer than the line breaks anywhere.
        constexpr size_t NoBreak = static_cast<size_t>(-1);
        size_t first = 0;
        size_t lastBreak = NoBreak;
        for (size_t i = 0; i < count; i++)
        {
            const bool space = isSpace(i);
            while (!space && i > first &&
                   GetGlyphX(shaped, spec, i + 1) - GetGlyphX(shaped, spec, first) > spec.MaxWidth)
            {
                const size_t breakAt = (lastBreak != NoBreak && lastBreak > first) ? lastBreak : i;
                addLine(first, breakAt);
                first = breakAt;
                lastBreak = NoBreak;
            }
            if (space)
                lastBreak = i + 1;
        }
        addLine(first, count);
    }

    Vec2 TextSystem::Measure(std::string_view text, const TextSpec& spec)
    {
        const FontMetrics metrics = GetMetrics(spec);
        const bool truncates = spec.MaxWidth > 0.0f && !spec.Wraps;
        Vec2 size;
        size_t start = 0;
        while (true)
        {
            const size_t end = text.find('\n', start);
            const std::string_view paragraph = text.substr(start, end == std::string_view::npos ? end : end - start);
            const ShapedLine& shaped = Shape(paragraph, spec);

            m_LineRanges.clear();
            BreakLines(paragraph, shaped, spec, m_LineRanges);
            for (const LineRange& line : m_LineRanges)
                size.X = std::max(size.X, truncates ? std::min(line.Width, spec.MaxWidth) : line.Width);
            size.Y += metrics.LineHeight * static_cast<float>(m_LineRanges.size());

            if (end == std::string_view::npos)
                break;
            start = end + 1;
        }
        return size;
    }

    void TextSystem::Draw(DrawList& drawList, Vec2 position, std::string_view text, const TextSpec& spec, Color color)
    {
        const FontMetrics metrics = GetMetrics(spec);
        const float alignment =
            spec.Alignment == TextAlignment::Center ? 0.5f : (spec.Alignment == TextAlignment::Trailing ? 1.0f : 0.0f);
        float y = position.Y;
        size_t start = 0;
        while (true)
        {
            const size_t end = text.find('\n', start);
            const std::string_view paragraph = text.substr(start, end == std::string_view::npos ? end : end - start);
            const ShapedLine& shaped = Shape(paragraph, spec);

            m_LineRanges.clear();
            BreakLines(paragraph, shaped, spec, m_LineRanges);
            for (size_t i = 0; i < m_LineRanges.size(); i++)
            {
                // Copied: drawing may shape an ellipsis, and nothing may alias the scratch vector.
                const LineRange line = m_LineRanges[i];
                const bool overflows = spec.MaxWidth > 0.0f && !spec.Wraps && line.Width > spec.MaxWidth;
                if (overflows)
                {
                    DrawTruncated(drawList, Vec2(position.X, y), shaped, spec, metrics, color);
                }
                else
                {
                    const float free = spec.MaxWidth > 0.0f ? std::max(0.0f, spec.MaxWidth - line.Width) : 0.0f;
                    const float x = position.X + free * alignment - line.Start;
                    DrawGlyphs(drawList, Vec2(x, y), shaped, line.First, line.End, spec, metrics, color);
                }
                y += metrics.LineHeight;
            }

            if (end == std::string_view::npos)
                break;
            start = end + 1;
        }
    }

    void TextSystem::GetCaretPositions(std::string_view line, const TextSpec& spec, std::vector<float>& positions)
    {
        const ShapedLine& shaped = Shape(line, spec);
        positions.assign(line.size() + 1, -1.0f);

        // The caret before the first byte of each cluster sits at the pen position of the cluster's first glyph.
        for (size_t i = 0; i < shaped.Glyphs.size(); i++)
        {
            const uint32_t cluster = shaped.Glyphs[i].Cluster;
            if (cluster < line.size() && positions[cluster] < 0.0f)
                positions[cluster] = GetGlyphX(shaped, spec, i);
        }
        positions[line.size()] = GetGlyphX(shaped, spec, shaped.Glyphs.size());
        if (positions[0] < 0.0f)
            positions[0] = 0.0f;

        // A cluster of several characters (a ligature, a letter with combining marks) is divided evenly
        // between its characters. Bytes inside a character share the character's position.
        size_t known = 0;
        while (known < line.size())
        {
            size_t next = known + 1;
            while (positions[next] < 0.0f)
                next++;

            size_t characters = 0;
            for (size_t offset = known; offset < next; offset = NextCodepointOffset(line, offset))
                characters++;
            const float step =
                (positions[next] - positions[known]) / static_cast<float>(std::max<size_t>(characters, 1));

            size_t character = 0;
            size_t characterStart = known;
            for (size_t offset = known + 1; offset < next; offset++)
            {
                if (offset == NextCodepointOffset(line, characterStart))
                {
                    character++;
                    characterStart = offset;
                }
                positions[offset] = positions[known] + step * static_cast<float>(character);
            }
            known = next;
        }
    }

    uint16_t TextSystem::AddFace(std::span<const uint8_t> data, bool copyData)
    {
        std::unique_ptr<FontFace> face;
        if (copyData)
            face = FontFace::Create(m_Library, {}, std::vector<uint8_t>(data.begin(), data.end()));
        else
            face = FontFace::Create(m_Library, data);
        if (face == nullptr || m_Faces.size() >= InvalidFace)
            return InvalidFace;
        m_Faces.push_back(std::move(face));
        return static_cast<uint16_t>(m_Faces.size() - 1);
    }

    uint16_t TextSystem::GetPrimaryFace(const TextSpec& spec) const
    {
        const Font* font = spec.Font != nullptr ? spec.Font : m_Fonts.front().get();
        return spec.Italic ? font->Italic : font->Roman;
    }

    uint16_t TextSystem::GetIconFace(const TextSpec& spec) const
    {
        switch (spec.Icons)
        {
            case IconVariant::Regular:
                return m_IconRegular;
            case IconVariant::Bold:
                return m_IconBold;
            case IconVariant::Fill:
                return m_IconFill;
            case IconVariant::Auto:
                break;
        }
        return spec.Weight >= FontWeight::Semibold ? m_IconBold : m_IconRegular;
    }

    uint16_t TextSystem::ResolveFace(char32_t codepoint, uint16_t primaryFace, uint16_t currentFace,
                                     const TextSpec& spec) const
    {
        if (m_Faces[primaryFace]->GetGlyphIndex(codepoint) != 0)
            return primaryFace;

        if (IsPrivateUse(codepoint))
        {
            const uint16_t iconFace = GetIconFace(spec);
            if (iconFace != InvalidFace && m_Faces[iconFace]->GetGlyphIndex(codepoint) != 0)
                return iconFace;
        }

        for (const std::unique_ptr<Font>& font : m_Fonts)
        {
            const uint16_t face = spec.Italic ? font->Italic : font->Roman;
            if (face != primaryFace && m_Faces[face]->GetGlyphIndex(codepoint) != 0)
                return face;
        }

        // Nobody has it (control characters, joiners, unsupported scripts): stay in the current run.
        return currentFace;
    }

    void TextSystem::ShapeRun(std::string_view line, size_t start, size_t length, uint16_t face, uint16_t primaryFace,
                              const TextSpec& spec, ShapedLine& shaped)
    {
        FontFace& fontFace = *m_Faces[face];
        const uint16_t weight = fontFace.ResolveWeight(static_cast<uint16_t>(spec.Weight));

        hb_buffer_clear_contents(m_Buffer);
        hb_buffer_add_utf8(m_Buffer, line.data(), static_cast<int>(line.size()), static_cast<unsigned int>(start),
                           static_cast<int>(length));
        hb_buffer_set_direction(m_Buffer, HB_DIRECTION_LTR);
        hb_buffer_guess_segment_properties(m_Buffer);
        hb_shape(fontFace.GetShapingFont(weight), m_Buffer, nullptr, 0);

        unsigned int glyphCount = 0;
        const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(m_Buffer, &glyphCount);
        const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(m_Buffer, &glyphCount);

        const bool isIcon = face == m_IconRegular || face == m_IconBold || face == m_IconFill;
        const float scale = isIcon ? IconScale : 1.0f;
        const float emPerUnit = scale / fontFace.GetUnitsPerEm();
        float baselineShift = 0.0f;
        if (isIcon)
        {
            const float capitalCenter = m_Faces[primaryFace]->GetCapHeight() * 0.5f;
            const float iconCenter = (fontFace.GetAscent() - fontFace.GetDescent()) * 0.5f * scale;
            baselineShift = capitalCenter - iconCenter;
        }

        float pen = shaped.Width;
        for (unsigned int i = 0; i < glyphCount; i++)
        {
            ShapedGlyph glyph;
            glyph.Glyph = infos[i].codepoint;
            glyph.Face = face;
            glyph.Weight = weight;
            glyph.X = pen + static_cast<float>(positions[i].x_offset) * emPerUnit;
            glyph.Y = static_cast<float>(positions[i].y_offset) * emPerUnit + baselineShift;
            glyph.Scale = scale;
            glyph.Cluster = infos[i].cluster;
            shaped.Glyphs.push_back(glyph);
            pen += static_cast<float>(positions[i].x_advance) * emPerUnit;
        }
        shaped.Width = pen;
    }

    const TextSystem::CachedGlyph* TextSystem::GetGlyph(const ShapedGlyph& glyph, float pixelSize, uint8_t subpixelBin)
    {
        GlyphKey key;
        key.Glyph = glyph.Glyph;
        key.PixelSize = static_cast<uint32_t>(std::lround(pixelSize * 64.0f));
        key.Face = glyph.Face;
        key.Weight = glyph.Weight;
        key.SubpixelBin = subpixelBin;

        const auto found = m_Glyphs.find(key);
        if (found != m_Glyphs.end())
            return &found->second;

        CachedGlyph cached;
        GlyphBitmap bitmap;
        const float subpixelX = static_cast<float>(subpixelBin) / static_cast<float>(SubpixelBins);
        if (m_Faces[glyph.Face]->Rasterize(glyph.Glyph, glyph.Weight, static_cast<float>(key.PixelSize) / 64.0f,
                                           subpixelX, bitmap))
        {
            if (!m_Atlas.Insert(bitmap.Width, bitmap.Height, bitmap.Pixels, bitmap.Pitch, cached.Region))
            {
                // The atlas is full at its maximum size. Skip this glyph now and start over next frame.
                if (!m_AtlasOverflowed)
                    CB_LOG_WARNING("Text", "Glyph atlas is full ({0} x {0}); it will be rebuilt", m_Atlas.GetWidth());
                m_AtlasOverflowed = true;
                return nullptr;
            }
            cached.Left = bitmap.Left;
            cached.Top = bitmap.Top;
        }
        // A glyph that failed to rasterize is cached as empty so it is not retried every frame.
        return &m_Glyphs.emplace(key, cached).first->second;
    }

    void TextSystem::DrawTruncated(DrawList& drawList, Vec2 position, const ShapedLine& shaped, const TextSpec& spec,
                                   const FontMetrics& metrics, Color color)
    {
        // Keep as many glyphs as fit next to an ellipsis, then draw the ellipsis.
        TextSpec ellipsisSpec = spec;
        ellipsisSpec.MaxWidth = 0.0f;
        const ShapedLine& ellipsis = Shape("\xE2\x80\xA6", ellipsisSpec);
        const float ellipsisWidth = ellipsis.Width * spec.Size;

        size_t visible = 0;
        while (visible < shaped.Glyphs.size() && GetGlyphX(shaped, spec, visible + 1) + ellipsisWidth <= spec.MaxWidth)
            visible++;

        DrawGlyphs(drawList, position, shaped, 0, visible, spec, metrics, color);
        const Vec2 ellipsisPosition(position.X + GetGlyphX(shaped, spec, visible), position.Y);
        DrawGlyphs(drawList, ellipsisPosition, ellipsis, 0, ellipsis.Glyphs.size(), ellipsisSpec, metrics, color);
    }

    void TextSystem::DrawGlyphs(DrawList& drawList, Vec2 position, const ShapedLine& shaped, size_t first, size_t end,
                                const TextSpec& spec, const FontMetrics& metrics, Color color)
    {
        if (first >= end)
            return;

        // Skip lines that are entirely clipped; glyphs may overhang the line box by a fraction of the size.
        const float startX = GetGlyphX(shaped, spec, first);
        const float width = GetGlyphX(shaped, spec, end) - startX;
        const Rect bounds = Rect(position.X + startX, position.Y, width, metrics.LineHeight).Expand(spec.Size);
        if (!drawList.GetClipRect().Intersects(bounds))
            return;

        // Glyph bitmaps are placed on whole pixels: the baseline is snapped vertically and the horizontal
        // position is quantized to the sub-pixel bins the glyphs are rasterized for.
        const float scale = drawList.GetContentScale();
        const float pixelsPerEm = spec.Size * scale;
        const float originX = position.X * scale;
        const float baselineY = std::round((position.Y + metrics.Baseline) * scale);

        for (size_t i = first; i < end; i++)
        {
            const ShapedGlyph& glyph = shaped.Glyphs[i];
            const float penX = originX + GetGlyphX(shaped, spec, i) * scale;

            const float quantized = std::floor(penX * static_cast<float>(SubpixelBins) + 0.5f);
            const float wholeX = std::floor(quantized / static_cast<float>(SubpixelBins));
            const uint8_t bin = static_cast<uint8_t>(quantized - wholeX * static_cast<float>(SubpixelBins));

            const CachedGlyph* cached = GetGlyph(glyph, pixelsPerEm * glyph.Scale, bin);
            if (cached == nullptr || cached->Region.Width == 0)
                continue;

            const float glyphBaseline = baselineY - std::round(glyph.Y * pixelsPerEm);
            const float left = wholeX + static_cast<float>(cached->Left);
            const float top = glyphBaseline - static_cast<float>(cached->Top);
            const Rect rect(left / scale, top / scale, static_cast<float>(cached->Region.Width) / scale,
                            static_cast<float>(cached->Region.Height) / scale);
            const Rect texels(static_cast<float>(cached->Region.X), static_cast<float>(cached->Region.Y),
                              static_cast<float>(cached->Region.Width), static_cast<float>(cached->Region.Height));
            drawList.AddGlyph(rect, texels, color);
        }
    }
} // namespace Carbon::Internal
