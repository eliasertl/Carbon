#include "Carbon/Text/Internal/TextSystem.h"

#include <algorithm>
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
        // With this many lines cached, a new line recycles the least recently used one instead of adding to them.
        constexpr size_t MaxIdleShapedLines = 1024;

        bool IsPrivateUse(char32_t codepoint)
        {
            return codepoint >= 0xE000 && codepoint <= 0xF8FF;
        }

        // Characters that join the one before them into an emoji, or choose how it is shown.
        constexpr char32_t ZeroWidthJoiner = 0x200D;
        constexpr char32_t TextPresentation = 0xFE0E;  // VS15
        constexpr char32_t EmojiPresentation = 0xFE0F; // VS16
        constexpr char32_t CombiningKeycap = 0x20E3;

        bool IsEmojiModifier(char32_t codepoint)
        {
            return codepoint >= 0x1F3FB && codepoint <= 0x1F3FF; // the skin tones
        }

        bool IsRegionalIndicator(char32_t codepoint)
        {
            return codepoint >= 0x1F1E6 && codepoint <= 0x1F1FF; // the letters of flags
        }

        bool IsTag(char32_t codepoint)
        {
            return codepoint >= 0xE0020 && codepoint <= 0xE007F; // subdivision flags (England, Scotland, Wales)
        }

        bool IsVariationSelector(char32_t codepoint)
        {
            return codepoint >= 0xFE00 && codepoint <= 0xFE0F;
        }

        // Characters with Unicode's Emoji_Presentation property: shown as emoji without VS16. Ranges of
        // emoji-data.txt (Unicode 16), merged where nothing in between is a character.
        constexpr std::pair<char32_t, char32_t> EmojiPresentationRanges[] = {
            {0x231A, 0x231B},   {0x23E9, 0x23EC},   {0x23F0, 0x23F0},   {0x23F3, 0x23F3},   {0x25FD, 0x25FE},
            {0x2614, 0x2615},   {0x2648, 0x2653},   {0x267F, 0x267F},   {0x2693, 0x2693},   {0x26A1, 0x26A1},
            {0x26AA, 0x26AB},   {0x26BD, 0x26BE},   {0x26C4, 0x26C5},   {0x26CE, 0x26CE},   {0x26D4, 0x26D4},
            {0x26EA, 0x26EA},   {0x26F2, 0x26F3},   {0x26F5, 0x26F5},   {0x26FA, 0x26FA},   {0x26FD, 0x26FD},
            {0x2705, 0x2705},   {0x270A, 0x270B},   {0x2728, 0x2728},   {0x274C, 0x274C},   {0x274E, 0x274E},
            {0x2753, 0x2755},   {0x2757, 0x2757},   {0x2795, 0x2797},   {0x27B0, 0x27B0},   {0x27BF, 0x27BF},
            {0x2B1B, 0x2B1C},   {0x2B50, 0x2B50},   {0x2B55, 0x2B55},   {0x1F004, 0x1F004}, {0x1F0CF, 0x1F0CF},
            {0x1F18E, 0x1F18E}, {0x1F191, 0x1F19A}, {0x1F1E6, 0x1F1FF}, {0x1F201, 0x1F201}, {0x1F21A, 0x1F21A},
            {0x1F22F, 0x1F22F}, {0x1F232, 0x1F236}, {0x1F238, 0x1F23A}, {0x1F250, 0x1F251}, {0x1F300, 0x1F320},
            {0x1F32D, 0x1F335}, {0x1F337, 0x1F37C}, {0x1F37E, 0x1F393}, {0x1F3A0, 0x1F3CA}, {0x1F3CF, 0x1F3D3},
            {0x1F3E0, 0x1F3F0}, {0x1F3F4, 0x1F3F4}, {0x1F3F8, 0x1F43E}, {0x1F440, 0x1F440}, {0x1F442, 0x1F4FC},
            {0x1F4FF, 0x1F53D}, {0x1F54B, 0x1F54E}, {0x1F550, 0x1F567}, {0x1F57A, 0x1F57A}, {0x1F595, 0x1F596},
            {0x1F5A4, 0x1F5A4}, {0x1F5FB, 0x1F64F}, {0x1F680, 0x1F6C5}, {0x1F6CC, 0x1F6CC}, {0x1F6D0, 0x1F6D2},
            {0x1F6D5, 0x1F6D7}, {0x1F6DC, 0x1F6DF}, {0x1F6EB, 0x1F6EC}, {0x1F6F4, 0x1F6FC}, {0x1F7E0, 0x1F7EB},
            {0x1F7F0, 0x1F7F0}, {0x1F90C, 0x1F93A}, {0x1F93C, 0x1F945}, {0x1F947, 0x1F9FF}, {0x1FA70, 0x1FA7C},
            {0x1FA80, 0x1FA89}, {0x1FA8F, 0x1FAC6}, {0x1FACE, 0x1FADC}, {0x1FADF, 0x1FAE9}, {0x1FAF0, 0x1FAF8},
        };

        bool IsEmojiPresentation(char32_t codepoint)
        {
            // Almost every character is below the first range; that answer costs one comparison.
            if (codepoint < EmojiPresentationRanges[0].first)
                return false;
            const auto after = std::upper_bound(
                std::begin(EmojiPresentationRanges), std::end(EmojiPresentationRanges), codepoint,
                [](char32_t value, const std::pair<char32_t, char32_t>& range) { return value < range.first; });
            return after != std::begin(EmojiPresentationRanges) && codepoint <= (after - 1)->second;
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
        m_MonospacedFont = LoadFont(GetEmbeddedFont(EmbeddedFont::JetBrainsMonoRoman),
                                    GetEmbeddedFont(EmbeddedFont::JetBrainsMonoItalic), "JetBrains Mono", false);
    }

    TextSystem::~TextSystem()
    {
        m_Fonts.clear();
        m_MonospacedFont.reset();
        m_Faces.clear();
        if (m_Buffer != nullptr)
            hb_buffer_destroy(m_Buffer);
        if (m_Library != nullptr)
            FT_Done_FreeType(m_Library);
    }

    Font* TextSystem::AddFont(std::span<const uint8_t> data, std::span<const uint8_t> italicData, std::string_view name,
                              bool copyData)
    {
        std::unique_ptr<Font> font = LoadFont(data, italicData, name, copyData);
        if (font == nullptr)
            return nullptr;
        m_Fonts.push_back(std::move(font));
        // Fallback resolution depends on the set of fonts, so cached lines are stale now.
        m_ShapedLines.clear();
        m_NewestLine = nullptr;
        m_OldestLine = nullptr;
        return m_Fonts.back().get();
    }

    std::unique_ptr<Font> TextSystem::LoadFont(std::span<const uint8_t> data, std::span<const uint8_t> italicData,
                                               std::string_view name, bool copyData)
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
        return font;
    }

    void TextSystem::BeginFrame(uint64_t frameCount, float contentScale)
    {
        m_FrameCount = frameCount;

        if (m_AtlasOverflowed || contentScale != m_ContentScale)
        {
            m_Atlas.Clear();
            if (m_ColorAtlas != nullptr)
                m_ColorAtlas->Clear();
            ClearGlyphs();
            m_AtlasOverflowed = false;
        }
        m_ContentScale = contentScale;

        if (frameCount % EvictionInterval == 0)
        {
            // The lines are in the order of their last use, so the stale ones are at the old end.
            while (m_OldestLine != nullptr && frameCount - m_OldestLine->LastUsedFrame > ShapedLineLifetime)
            {
                ShapedLine& line = *m_OldestLine;
                const uint64_t key = line.Key;
                UnlinkLine(line);
                m_ShapedLines.erase(key);
            }
        }
    }

    void TextSystem::TouchLine(ShapedLine& line)
    {
        // The order among the lines of one frame does not matter: a line moves once per frame.
        if (line.LastUsedFrame == m_FrameCount)
            return;
        line.LastUsedFrame = m_FrameCount;
        if (m_NewestLine != &line)
        {
            UnlinkLine(line);
            LinkNewest(line);
        }
    }

    void TextSystem::LinkNewest(ShapedLine& line)
    {
        line.Newer = nullptr;
        line.Older = m_NewestLine;
        if (m_NewestLine != nullptr)
            m_NewestLine->Newer = &line;
        m_NewestLine = &line;
        if (m_OldestLine == nullptr)
            m_OldestLine = &line;
    }

    void TextSystem::UnlinkLine(ShapedLine& line)
    {
        (line.Newer != nullptr ? line.Newer->Older : m_NewestLine) = line.Older;
        (line.Older != nullptr ? line.Older->Newer : m_OldestLine) = line.Newer;
        line.Newer = nullptr;
        line.Older = nullptr;
    }

    ShapedLine& TextSystem::AcquireLine(uint64_t key)
    {
        // A line used in this frame is never recycled: callers hold references until the frame ends.
        ShapedLine* line = nullptr;
        if (m_ShapedLines.size() >= MaxIdleShapedLines && m_OldestLine != nullptr &&
            m_OldestLine->LastUsedFrame != m_FrameCount)
        {
            // The node moves to its new key with the glyph storage it has, so nothing is allocated.
            ShapedLine& oldest = *m_OldestLine;
            const uint64_t oldestKey = oldest.Key;
            UnlinkLine(oldest);
            auto node = m_ShapedLines.extract(oldestKey);
            node.key() = key;
            line = &m_ShapedLines.insert(std::move(node)).position->second;
            line->Glyphs.clear();
            line->Slots.clear();
            line->Width = 0.0f;
        }
        else
        {
            line = &m_ShapedLines.emplace(key, ShapedLine()).first->second;
        }
        line->Key = key;
        line->LastUsedFrame = m_FrameCount;
        LinkNewest(*line);
        return *line;
    }

    void TextSystem::SetMaxAtlasSize(uint32_t size)
    {
        const uint32_t maxSize = std::clamp(size, InitialAtlasSize, MaxAtlasSize);
        // Either atlas shrinking empties both, since the glyph cache refers to both.
        bool isCleared = m_Atlas.SetMaxSize(maxSize);
        if (m_ColorAtlas != nullptr && m_ColorAtlas->SetMaxSize(maxSize))
            isCleared = true;
        if (isCleared)
        {
            m_Atlas.Clear();
            if (m_ColorAtlas != nullptr)
                m_ColorAtlas->Clear();
            ClearGlyphs();
        }
    }

    void TextSystem::ClearGlyphs()
    {
        m_Glyphs.clear();
        m_GlyphEpoch++;
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
            TouchLine(found->second);
            return found->second;
        }

        ShapedLine& shaped = AcquireLine(key);
        // One glyph per byte is the most a line shapes to in practice; the storage is reused when it is recycled.
        shaped.Glyphs.reserve(line.size());

        // Split the line into runs that use the same face, then shape each run. A character and the characters
        // that join it (an emoji sequence) choose their face together.
        const uint16_t primaryFace = GetPrimaryFace(spec);
        uint16_t runFace = primaryFace;
        size_t runStart = 0;
        size_t offset = 0;
        while (offset < line.size())
        {
            const FallbackCluster cluster = FindFallbackCluster(line, offset);
            const uint16_t face = ResolveFace(cluster, primaryFace, runFace, spec);
            if (face != runFace)
            {
                if (offset > runStart)
                    ShapeRun(line, runStart, offset - runStart, runFace, primaryFace, spec, shaped);
                runFace = face;
                runStart = offset;
            }
            offset = cluster.End;
        }
        if (offset > runStart)
            ShapeRun(line, runStart, offset - runStart, runFace, primaryFace, spec, shaped);

        return shaped;
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

        // Text outside the clip rectangle is rejected here, before it is hashed, shaped and broken into lines:
        // in a long scrolling list that is most of the text. Lines run downwards from `position`, so text that
        // starts below the clip rectangle is out, and so is text that ends above it when its number of lines is
        // known without breaking them. The margin is the one DrawGlyphs allows for glyphs that overhang their line.
        const Rect& clipRect = drawList.GetClipRect();
        if (position.Y - spec.Size >= clipRect.GetBottom())
            return;
        if (spec.MaxWidth <= 0.0f || !spec.Wraps)
        {
            const float lineCount = static_cast<float>(1 + std::count(text.begin(), text.end(), '\n'));
            if (position.Y + metrics.LineHeight * lineCount + spec.Size <= clipRect.Y)
                return;
        }

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

    TextSystem::FallbackCluster TextSystem::FindFallbackCluster(std::string_view line, size_t offset)
    {
        FallbackCluster cluster;
        const UTF8Decoded base = DecodeUTF8(line, offset);
        cluster.Base = base.Codepoint;
        cluster.End = offset + base.Length;
        cluster.WantsColor = IsEmojiPresentation(base.Codepoint);
        bool isPairedFlag = false;
        while (cluster.End < line.size())
        {
            const UTF8Decoded next = DecodeUTF8(line, cluster.End);
            const char32_t codepoint = next.Codepoint;
            if (codepoint == ZeroWidthJoiner)
            {
                // The joiner and the character after it belong to the sequence, which goes on from there.
                cluster.End += next.Length;
                if (cluster.End < line.size())
                    cluster.End += DecodeUTF8(line, cluster.End).Length;
                continue;
            }
            if (IsVariationSelector(codepoint) || IsEmojiModifier(codepoint) || IsTag(codepoint) ||
                codepoint == CombiningKeycap)
            {
                if (codepoint == EmojiPresentation || IsEmojiModifier(codepoint) || IsTag(codepoint) ||
                    codepoint == CombiningKeycap)
                    cluster.WantsColor = true;
                if (codepoint == TextPresentation)
                    cluster.WantsText = true;
                cluster.End += next.Length;
                continue;
            }
            // Two regional indicators are one flag.
            if (IsRegionalIndicator(base.Codepoint) && IsRegionalIndicator(codepoint) && !isPairedFlag)
            {
                isPairedFlag = true;
                cluster.End += next.Length;
                continue;
            }
            break;
        }
        // VS15 asks for text presentation even of a character that is an emoji by default.
        if (cluster.WantsText)
            cluster.WantsColor = false;
        return cluster;
    }

    uint16_t TextSystem::ResolveFace(const FallbackCluster& cluster, uint16_t primaryFace, uint16_t currentFace,
                                     const TextSpec& spec) const
    {
        const char32_t codepoint = cluster.Base;

        // Carbon::Icons live in the Private Use Area, where some fonts have glyphs of their own (JetBrains Mono's
        // powerline symbols share code points with Phosphor). An icon must look the same in every font, so the
        // icon font comes first there.
        if (IsPrivateUse(codepoint))
        {
            const uint16_t iconFace = GetIconFace(spec);
            if (iconFace != InvalidFace && m_Faces[iconFace]->GetGlyphIndex(codepoint) != 0)
                return iconFace;
        }

        // The faces in the order they are tried: the requested one, then the fonts in the order they were added.
        // An emoji tries those with color glyphs first, so that a font with a plain version of it (many CJK fonts
        // have some) does not take it from the emoji font added after it; VS15 asks for the plain ones first.
        const auto findFace = [&](bool withColor, bool withoutColor)
        {
            const auto accepts = [&](uint16_t face)
            {
                const bool hasColor = m_Faces[face]->HasColorGlyphs();
                return ((hasColor && withColor) || (!hasColor && withoutColor)) &&
                       m_Faces[face]->GetGlyphIndex(codepoint) != 0;
            };
            if (accepts(primaryFace))
                return primaryFace;
            for (const std::unique_ptr<Font>& font : m_Fonts)
            {
                const uint16_t face = spec.Italic ? font->Italic : font->Roman;
                if (face != primaryFace && accepts(face))
                    return face;
            }
            return InvalidFace;
        };
        uint16_t face = InvalidFace;
        if (cluster.WantsColor)
            face = findFace(true, false);
        else if (cluster.WantsText)
            face = findFace(false, true);
        if (face == InvalidFace)
            face = findFace(true, true);

        // Nobody has it (control characters, joiners, unsupported scripts): stay in the current run.
        return face != InvalidFace ? face : currentFace;
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
            glyph.IsColor = fontFace.HasColorGlyphs() && fontFace.IsColorGlyph(glyph.Glyph, weight);
            shaped.Glyphs.push_back(glyph);
            pen += static_cast<float>(positions[i].x_advance) * emPerUnit;
        }
        shaped.Width = pen;
    }

    const CachedGlyph* TextSystem::GetGlyph(const ShapedGlyph& glyph, uint32_t pixelSize, uint8_t subpixelBin)
    {
        GlyphKey key;
        key.Glyph = glyph.Glyph;
        key.PixelSize = pixelSize;
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
            // Color glyphs go into an atlas of their own, made when the first one appears.
            cached.IsColor = bitmap.Format == GlyphFormat::Color;
            if (cached.IsColor && m_ColorAtlas == nullptr)
                m_ColorAtlas =
                    std::make_unique<GlyphAtlas>(InitialAtlasSize, InitialAtlasSize, m_Atlas.GetMaxSize(), 4);
            GlyphAtlas& atlas = cached.IsColor ? *m_ColorAtlas : m_Atlas;
            if (!atlas.Insert(bitmap.Width, bitmap.Height, bitmap.Pixels, bitmap.Pitch, cached.Region))
            {
                // The atlas is full at its maximum size. Skip this glyph now and start over next frame.
                if (!m_AtlasOverflowed)
                    CB_LOG_WARNING("Text", "Glyph atlas is full ({0} x {0}); it will be rebuilt", atlas.GetWidth());
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

        // The glyphs are looked up in the glyph cache once and remembered with the line; see GlyphSlot.
        if (shaped.SlotEpoch != m_GlyphEpoch || shaped.Slots.size() != shaped.Glyphs.size())
        {
            shaped.Slots.assign(shaped.Glyphs.size(), GlyphSlot());
            shaped.SlotEpoch = m_GlyphEpoch;
        }
        const uint32_t pixelSize = static_cast<uint32_t>(std::lround(pixelsPerEm * 64.0f));

        // Color glyphs are drawn in their own colors, faded as the text is: only the color's opacity applies.
        const Color colorGlyphTint = Color::White().WithOpacity(color.A);

        for (size_t i = first; i < end; i++)
        {
            const ShapedGlyph& glyph = shaped.Glyphs[i];
            const float penX = originX + GetGlyphX(shaped, spec, i) * scale;

            // Color glyphs are pictures rather than strokes; one rasterization on the nearest pixel is enough.
            const float quantized = glyph.IsColor ? std::round(penX) * static_cast<float>(SubpixelBins)
                                                  : std::floor(penX * static_cast<float>(SubpixelBins) + 0.5f);
            const float wholeX = std::floor(quantized / static_cast<float>(SubpixelBins));
            const uint8_t bin = static_cast<uint8_t>(quantized - wholeX * static_cast<float>(SubpixelBins));

            // Icons are drawn larger than the text around them.
            const uint32_t glyphPixelSize =
                glyph.Scale == 1.0f ? pixelSize : static_cast<uint32_t>(std::lround(pixelsPerEm * glyph.Scale * 64.0f));
            GlyphSlot& slot = shaped.Slots[i];
            if (slot.Glyph == nullptr || slot.PixelSize != glyphPixelSize || slot.SubpixelBin != bin)
            {
                // A glyph that does not fit into the atlas is looked up again in the next frame.
                slot.Glyph = GetGlyph(glyph, glyphPixelSize, bin);
                slot.PixelSize = glyphPixelSize;
                slot.SubpixelBin = bin;
            }
            const CachedGlyph* cached = slot.Glyph;
            if (cached == nullptr || cached->Region.Width == 0)
                continue;

            const float glyphBaseline = baselineY - std::round(glyph.Y * pixelsPerEm);
            const float left = wholeX + static_cast<float>(cached->Left);
            const float top = glyphBaseline - static_cast<float>(cached->Top);
            const Rect rect(left / scale, top / scale, static_cast<float>(cached->Region.Width) / scale,
                            static_cast<float>(cached->Region.Height) / scale);
            const Rect texels(static_cast<float>(cached->Region.X), static_cast<float>(cached->Region.Y),
                              static_cast<float>(cached->Region.Width), static_cast<float>(cached->Region.Height));
            if (cached->IsColor)
                drawList.AddColorGlyph(rect, texels, colorGlyphTint);
            else
                drawList.AddGlyph(rect, texels, color);
        }
    }
} // namespace Carbon::Internal
