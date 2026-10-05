#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Carbon/Core/Color.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Text/Font.h"
#include "Carbon/Text/Internal/FontFace.h"
#include "Carbon/Text/Internal/GlyphAtlas.h"
#include "Carbon/Text/TextSpec.h"

typedef struct hb_buffer_t hb_buffer_t;

namespace Carbon
{
    class DrawList;

    /// A font family: indices into the text system's faces.
    struct Font
    {
        std::string Name;
        uint16_t Roman = 0;
        uint16_t Italic = 0;
    };
} // namespace Carbon

namespace Carbon::Internal
{
    /// One positioned glyph of a shaped line. Positions are in ems of the text size, so a shaped line can be
    /// drawn at any size and content scale.
    struct ShapedGlyph
    {
        uint32_t Glyph = 0;
        uint16_t Face = 0;
        uint16_t Weight = 0;
        /// Pen position of the glyph relative to the start of the line.
        float X = 0.0f;
        /// Shift above the baseline.
        float Y = 0.0f;
        /// Size multiplier; icons are drawn slightly larger than the surrounding text.
        float Scale = 1.0f;
        /// Byte offset, in the shaped line, of the first character this glyph belongs to.
        uint32_t Cluster = 0;
    };

    /// A shaped line of text, cached between frames.
    struct ShapedLine
    {
        std::vector<ShapedGlyph> Glyphs;
        /// Advance width in ems.
        float Width = 0.0f;
        uint64_t LastUsedFrame = 0;
    };

    /// Fonts, shaping, glyph rasterization and the glyph atlas of one context. GPU-free: the renderer only
    /// uploads the atlas pixels.
    class TextSystem
    {
    public:
        TextSystem();
        ~TextSystem();

        TextSystem(const TextSystem&) = delete;
        TextSystem& operator=(const TextSystem&) = delete;

        /// Registers a font family. With `copyData` the bytes are copied, otherwise they must outlive the system.
        Font* AddFont(std::span<const uint8_t> data, std::span<const uint8_t> italicData, std::string_view name,
                      bool copyData);
        Font* GetDefaultFont() { return m_Fonts.empty() ? nullptr : m_Fonts.front().get(); }
        Font* GetMonospacedFont() { return m_MonospacedFont.get(); }

        /// Called by NewFrame: evicts stale cache entries and resets the atlas when it overflowed or the content
        /// scale changed.
        void BeginFrame(uint64_t frameCount, float contentScale);

        /// Shapes one line (no '\n'). The result is cached and stays valid until the next BeginFrame.
        const ShapedLine& Shape(std::string_view line, const TextSpec& spec);

        FontMetrics GetMetrics(const TextSpec& spec);
        Vec2 Measure(std::string_view text, const TextSpec& spec);
        void Draw(DrawList& drawList, Vec2 position, std::string_view text, const TextSpec& spec, Color color);

        /// Fills `positions` with the horizontal caret position, in points, before every byte of a line and
        /// after its last one (size + 1 entries).
        void GetCaretPositions(std::string_view line, const TextSpec& spec, std::vector<float>& positions);

        GlyphAtlas& GetAtlas() { return m_Atlas; }
        size_t GetShapedLineCount() const { return m_ShapedLines.size(); }
        size_t GetCachedGlyphCount() const { return m_Glyphs.size(); }

    private:
        struct GlyphKey
        {
            uint32_t Glyph = 0;
            uint32_t PixelSize = 0; // 26.6 fixed point
            uint16_t Face = 0;
            uint16_t Weight = 0;
            uint8_t SubpixelBin = 0;

            bool operator==(const GlyphKey& other) const = default;
        };

        struct GlyphKeyHash
        {
            size_t operator()(const GlyphKey& key) const noexcept;
        };

        /// A visual line: a range of glyphs of a shaped paragraph.
        struct LineRange
        {
            size_t First = 0;
            size_t End = 0;
            /// Pen position of the first glyph and width without trailing spaces, in points.
            float Start = 0.0f;
            float Width = 0.0f;
        };

        struct CachedGlyph
        {
            AtlasRegion Region;
            int32_t Left = 0;
            int32_t Top = 0;
        };

        uint16_t AddFace(std::span<const uint8_t> data, bool copyData);
        std::unique_ptr<Font> LoadFont(std::span<const uint8_t> data, std::span<const uint8_t> italicData,
                                       std::string_view name, bool copyData);
        uint16_t GetPrimaryFace(const TextSpec& spec) const;
        uint16_t GetIconFace(const TextSpec& spec) const;
        uint16_t ResolveFace(char32_t codepoint, uint16_t primaryFace, uint16_t currentFace,
                             const TextSpec& spec) const;
        void ShapeRun(std::string_view line, size_t start, size_t length, uint16_t face, uint16_t primaryFace,
                      const TextSpec& spec, ShapedLine& shaped);
        const CachedGlyph* GetGlyph(const ShapedGlyph& glyph, float pixelSize, uint8_t subpixelBin);
        /// Pen position of a glyph in points; for index == glyph count, the end of the line.
        static float GetGlyphX(const ShapedLine& shaped, const TextSpec& spec, size_t index);
        /// Splits a shaped paragraph into visual lines no wider than the spec's MaxWidth.
        void BreakLines(std::string_view paragraph, const ShapedLine& shaped, const TextSpec& spec,
                        std::vector<LineRange>& lines);
        void DrawGlyphs(DrawList& drawList, Vec2 position, const ShapedLine& shaped, size_t first, size_t end,
                        const TextSpec& spec, const FontMetrics& metrics, Color color);
        void DrawTruncated(DrawList& drawList, Vec2 position, const ShapedLine& shaped, const TextSpec& spec,
                           const FontMetrics& metrics, Color color);

    private:
        FT_Library m_Library = nullptr;
        hb_buffer_t* m_Buffer = nullptr;
        std::vector<std::unique_ptr<FontFace>> m_Faces;
        /// The default font first, then the fonts the host added: the fallback order.
        std::vector<std::unique_ptr<Font>> m_Fonts;
        /// The embedded monospaced font. Kept out of m_Fonts so it is never a fallback for other fonts.
        std::unique_ptr<Font> m_MonospacedFont;
        uint16_t m_IconRegular = 0;
        uint16_t m_IconBold = 0;
        uint16_t m_IconFill = 0;

        GlyphAtlas m_Atlas;
        std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> m_Glyphs;
        std::unordered_map<uint64_t, ShapedLine> m_ShapedLines;
        uint64_t m_FrameCount = 0;
        float m_ContentScale = 1.0f;
        bool m_AtlasOverflowed = false;
        /// Scratch storage for line breaking, reused between calls.
        std::vector<LineRange> m_LineRanges;
    };
} // namespace Carbon::Internal
