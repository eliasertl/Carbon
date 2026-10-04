#include "Support/ContextTest.h"

#include <string>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Text/Internal/TextSystem.h"

namespace Carbon
{
    class TextTests : public ContextTest
    {
    protected:
        Internal::TextSystem& GetTextSystem() { return *Internal::GetContext().Text; }

        float GetWidth(std::string_view text, const TextSpec& spec = {}) { return MeasureText(text, spec).X; }
    };

    TEST_F(TextTests, DefaultFontIsAvailable)
    {
        EXPECT_NE(GetDefaultFont(), nullptr);
        EXPECT_GT(GetWidth("Carbon"), 0.0f);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(TextTests, WidthScalesLinearlyWithSize)
    {
        TextSpec small;
        small.Size = 13.0f;
        TextSpec large;
        large.Size = 26.0f;
        EXPECT_NEAR(GetWidth("Hamburgefons", large), GetWidth("Hamburgefons", small) * 2.0f, 1e-3f);
    }

    TEST_F(TextTests, MeasurementDoesNotDependOnContentScale)
    {
        const float width = GetWidth("Layout is in points");
        GetIO().SetContentScale(2.0f);
        RunFrame();
        EXPECT_FLOAT_EQ(GetWidth("Layout is in points"), width);
    }

    TEST_F(TextTests, KerningTightensPairs)
    {
        TextSpec spec;
        spec.Size = 100.0f;
        // "AV" is the classic kerning pair: shaped together it is narrower than its letters side by side.
        const float separate = GetWidth("A", spec) + GetWidth("V", spec);
        const float pair = GetWidth("AV", spec);
        EXPECT_LT(pair, separate - 1.0f);
        // A pair without kerning is exactly additive.
        EXPECT_NEAR(GetWidth("HH", spec), GetWidth("H", spec) * 2.0f, 1e-3f);
    }

    TEST_F(TextTests, ShapesOneGlyphPerCharacterForPlainText)
    {
        const Internal::ShapedLine& shaped = GetTextSystem().Shape("Hello", TextSpec());
        ASSERT_EQ(shaped.Glyphs.size(), 5u);
        // Glyphs advance left to right.
        for (size_t i = 1; i < shaped.Glyphs.size(); i++)
            EXPECT_GT(shaped.Glyphs[i].X, shaped.Glyphs[i - 1].X);
        // "l" and "l" are the same glyph.
        EXPECT_EQ(shaped.Glyphs[2].Glyph, shaped.Glyphs[3].Glyph);
        EXPECT_NE(shaped.Glyphs[0].Glyph, 0u);
    }

    TEST_F(TextTests, ShapesMultiByteCharacters)
    {
        // "Größe €5": 8 characters, 11 bytes.
        const std::string text =
            "Gr\xC3\xB6\xC3\x9F"
            "e \xE2\x82\xAC"
            "5";
        const Internal::ShapedLine& shaped = GetTextSystem().Shape(text, TextSpec());
        EXPECT_EQ(shaped.Glyphs.size(), 8u);
        for (const Internal::ShapedGlyph& glyph : shaped.Glyphs)
            EXPECT_NE(glyph.Glyph, 0u) << "Public Sans should cover Latin-1 and the euro sign";
    }

    TEST_F(TextTests, HeavierWeightsAreWider)
    {
        TextSpec regular;
        TextSpec bold;
        bold.Weight = FontWeight::Bold;
        TextSpec thin;
        thin.Weight = FontWeight::Thin;
        const float regularWidth = GetWidth("Variable weight", regular);
        EXPECT_GT(GetWidth("Variable weight", bold), regularWidth);
        EXPECT_LT(GetWidth("Variable weight", thin), regularWidth);
    }

    TEST_F(TextTests, ItalicUsesItsOwnFace)
    {
        TextSpec italic;
        italic.Italic = true;
        const Internal::ShapedLine& upright = GetTextSystem().Shape("a", TextSpec());
        const Internal::ShapedLine& slanted = GetTextSystem().Shape("a", italic);
        ASSERT_EQ(upright.Glyphs.size(), 1u);
        ASSERT_EQ(slanted.Glyphs.size(), 1u);
        EXPECT_NE(upright.Glyphs[0].Face, slanted.Glyphs[0].Face);
    }

    TEST_F(TextTests, TrackingAddsSpacePerGlyph)
    {
        TextSpec tracked;
        tracked.Tracking = 2.0f;
        EXPECT_NEAR(GetWidth("abcd", tracked), GetWidth("abcd") + 8.0f, 1e-3f);
    }

    TEST_F(TextTests, IconsFallBackToTheIconFont)
    {
        const std::string text = std::string("Home ") + Icons::House;
        const Internal::ShapedLine& shaped = GetTextSystem().Shape(text, TextSpec());
        ASSERT_EQ(shaped.Glyphs.size(), 6u);

        const Internal::ShapedGlyph& letter = shaped.Glyphs[0];
        const Internal::ShapedGlyph& icon = shaped.Glyphs[5];
        EXPECT_NE(icon.Glyph, 0u);
        EXPECT_NE(icon.Face, letter.Face);
        // Icons are drawn larger than the text around them.
        EXPECT_GT(icon.Scale, 1.0f);
        EXPECT_FLOAT_EQ(letter.Scale, 1.0f);
        // The icon takes up space of its own.
        EXPECT_GT(GetWidth(text), GetWidth("Home ") + 5.0f);
    }

    TEST_F(TextTests, IconWeightFollowsTextWeightUnlessOverridden)
    {
        TextSpec regular;
        TextSpec bold;
        bold.Weight = FontWeight::Bold;
        TextSpec filled;
        filled.Icons = IconVariant::Fill;
        TextSpec boldButRegularIcons = bold;
        boldButRegularIcons.Icons = IconVariant::Regular;

        Internal::TextSystem& text = GetTextSystem();
        const uint16_t regularFace = text.Shape(Icons::Gear, regular).Glyphs.at(0).Face;
        const uint16_t boldFace = text.Shape(Icons::Gear, bold).Glyphs.at(0).Face;
        const uint16_t fillFace = text.Shape(Icons::Gear, filled).Glyphs.at(0).Face;
        EXPECT_NE(regularFace, boldFace);
        EXPECT_NE(regularFace, fillFace);
        EXPECT_NE(boldFace, fillFace);
        EXPECT_EQ(text.Shape(Icons::Gear, boldButRegularIcons).Glyphs.at(0).Face, regularFace);
    }

    TEST_F(TextTests, MultipleLinesStackVertically)
    {
        TextSpec spec = GetTextSpec(TextStyle::Body);
        const Vec2 one = MeasureText("First line", spec);
        const Vec2 two = MeasureText("First line\nSecond, longer line", spec);
        EXPECT_FLOAT_EQ(one.Y, 16.0f);
        EXPECT_FLOAT_EQ(two.Y, 32.0f);
        EXPECT_FLOAT_EQ(two.X, GetWidth("Second, longer line", spec));
        // An empty string still occupies one line.
        EXPECT_FLOAT_EQ(MeasureText("", spec).Y, 16.0f);
        EXPECT_FLOAT_EQ(MeasureText("", spec).X, 0.0f);
    }

    TEST_F(TextTests, MetricsCenterTheGlyphsInTheLine)
    {
        const TextSpec spec = GetTextSpec(TextStyle::Body);
        const FontMetrics metrics = GetFontMetrics(spec);
        EXPECT_FLOAT_EQ(metrics.LineHeight, 16.0f);
        EXPECT_GT(metrics.Ascent, metrics.CapHeight);
        EXPECT_GT(metrics.CapHeight, 0.5f * spec.Size);
        EXPECT_GT(metrics.Descent, 0.0f);
        // The space above the ascent equals the space below the descent.
        EXPECT_NEAR(metrics.Baseline - metrics.Ascent, metrics.LineHeight - metrics.Baseline - metrics.Descent, 1e-4f);

        // Without an explicit line height the font's natural one is used.
        TextSpec natural;
        natural.Size = 20.0f;
        const FontMetrics naturalMetrics = GetFontMetrics(natural);
        EXPECT_GE(naturalMetrics.LineHeight, naturalMetrics.Ascent + naturalMetrics.Descent - 1e-4f);
    }

    TEST_F(TextTests, TypeRampMatchesTheHIG)
    {
        struct Expected
        {
            TextStyle Style;
            float Size;
            float LineHeight;
            FontWeight Weight;
            FontWeight Emphasized;
        };
        const Expected ramp[] = {
            {TextStyle::LargeTitle, 26.0f, 32.0f, FontWeight::Regular, FontWeight::Bold},
            {TextStyle::Title1, 22.0f, 26.0f, FontWeight::Regular, FontWeight::Bold},
            {TextStyle::Title2, 17.0f, 22.0f, FontWeight::Regular, FontWeight::Bold},
            {TextStyle::Title3, 15.0f, 20.0f, FontWeight::Regular, FontWeight::Semibold},
            {TextStyle::Headline, 13.0f, 16.0f, FontWeight::Bold, FontWeight::Heavy},
            {TextStyle::Body, 13.0f, 16.0f, FontWeight::Regular, FontWeight::Semibold},
            {TextStyle::Callout, 12.0f, 15.0f, FontWeight::Regular, FontWeight::Semibold},
            {TextStyle::Subheadline, 11.0f, 14.0f, FontWeight::Regular, FontWeight::Semibold},
            {TextStyle::Footnote, 10.0f, 13.0f, FontWeight::Regular, FontWeight::Semibold},
            {TextStyle::Caption1, 10.0f, 13.0f, FontWeight::Regular, FontWeight::Medium},
            {TextStyle::Caption2, 10.0f, 13.0f, FontWeight::Medium, FontWeight::Semibold},
        };
        for (const Expected& expected : ramp)
        {
            const TextSpec spec = GetTextSpec(expected.Style);
            EXPECT_FLOAT_EQ(spec.Size, expected.Size);
            EXPECT_FLOAT_EQ(spec.LineHeight, expected.LineHeight);
            EXPECT_EQ(spec.Weight, expected.Weight);
            EXPECT_EQ(GetTextSpec(expected.Style, true).Weight, expected.Emphasized);
        }
    }

    TEST_F(TextTests, ShapedLinesAreCached)
    {
        Internal::TextSystem& text = GetTextSystem();
        const size_t before = text.GetShapedLineCount();
        const Internal::ShapedLine* first = &text.Shape("Cache me", TextSpec());
        const Internal::ShapedLine* second = &text.Shape("Cache me", TextSpec());
        EXPECT_EQ(first, second);
        EXPECT_EQ(text.GetShapedLineCount(), before + 1);

        // A different weight is a different entry.
        TextSpec bold;
        bold.Weight = FontWeight::Bold;
        EXPECT_NE(&text.Shape("Cache me", bold), first);
        EXPECT_EQ(text.GetShapedLineCount(), before + 2);
    }

    TEST_F(TextTests, StaleShapedLinesAreEvicted)
    {
        Internal::TextSystem& text = GetTextSystem();
        text.Shape("Short-lived", TextSpec());
        const size_t withEntry = text.GetShapedLineCount();
        for (int i = 0; i < 900; i++)
            RunFrame();
        EXPECT_LT(text.GetShapedLineCount(), withEntry);
    }

    TEST_F(TextTests, DrawingEmitsPixelAlignedGlyphQuads)
    {
        GetIO().SetContentScale(1.5f);
        NewFrame();
        DrawList& drawList = GetDrawList();
        drawList.AddText(Vec2(20.3f, 40.7f), "Crisp", GetTextSpec(TextStyle::Body), Color::Black());

        const std::span<const DrawVertex> vertices = drawList.GetVertices();
        ASSERT_EQ(vertices.size(), 5u * 4u);
        for (const DrawVertex& vertex : vertices)
        {
            // Every corner of every glyph quad sits on a whole pixel.
            const Vec2 pixels = vertex.Position * 1.5f;
            EXPECT_NEAR(pixels.X, std::round(pixels.X), 1e-3f);
            EXPECT_NEAR(pixels.Y, std::round(pixels.Y), 1e-3f);
            EXPECT_EQ(drawList.GetPrimitives()[vertex.Primitive].Kind, DrawPrimitiveKind::Glyph);
        }
        // The quads are in the right place: near the requested origin, inside the 16-point line.
        EXPECT_NEAR(vertices[0].Position.X, 20.3f, 1.5f);
        EXPECT_GT(vertices[0].Position.Y, 40.0f);
        EXPECT_LT(vertices[2].Position.Y, 40.7f + 16.0f);

        // One command, bound to the glyph atlas.
        ASSERT_EQ(drawList.GetCommands(DrawLayer::Content).size(), 1u);
        EXPECT_EQ(drawList.GetCommands(DrawLayer::Content)[0].Texture, TextureID());
        EXPECT_TRUE(GetTextSystem().GetAtlas().IsDirty());
        EndFrame();
    }

    TEST_F(TextTests, GlyphsAreRasterizedOncePerSizeAndScale)
    {
        Internal::TextSystem& text = GetTextSystem();
        const TextSpec spec = GetTextSpec(TextStyle::Body);

        NewFrame();
        GetDrawList().AddText(Vec2(10.0f, 10.0f), "aaaa", spec, Color::Black());
        EndFrame();
        const size_t afterFirstFrame = text.GetCachedGlyphCount();
        EXPECT_GE(afterFirstFrame, 1u);
        EXPECT_LE(afterFirstFrame, 4u); // one glyph, at most four sub-pixel positions

        // Drawing the same text again adds nothing.
        NewFrame();
        GetDrawList().AddText(Vec2(10.0f, 10.0f), "aaaa", spec, Color::Black());
        EndFrame();
        EXPECT_EQ(text.GetCachedGlyphCount(), afterFirstFrame);

        // A new content scale discards the old bitmaps and rasterizes at the new pixel size.
        GetIO().SetContentScale(2.0f);
        NewFrame();
        EXPECT_EQ(text.GetCachedGlyphCount(), 0u);
        GetDrawList().AddText(Vec2(10.0f, 10.0f), "aaaa", spec, Color::Black());
        EndFrame();
        EXPECT_GE(text.GetCachedGlyphCount(), 1u);
    }

    TEST_F(TextTests, ClippedTextIsSkipped)
    {
        NewFrame();
        DrawList& drawList = GetDrawList();
        drawList.PushClipRect(Rect(0.0f, 0.0f, 100.0f, 100.0f));
        drawList.AddText(Vec2(10.0f, 400.0f), "Out of sight", GetTextSpec(TextStyle::Body), Color::Black());
        drawList.PopClipRect();
        EXPECT_TRUE(drawList.GetVertices().empty());
        EndFrame();
    }

    TEST_F(TextTests, UnknownCharactersDoNotBreakShaping)
    {
        // CJK is not covered by the embedded fonts: the glyphs are missing, but shaping and drawing still work.
        const std::string text =
            "A\xE4\xB8\xAD\xE6\x96\x87"
            "B";
        const Internal::ShapedLine& shaped = GetTextSystem().Shape(text, TextSpec());
        EXPECT_EQ(shaped.Glyphs.size(), 4u);
        NewFrame();
        GetDrawList().AddText(Vec2(10.0f, 10.0f), text, TextSpec(), Color::Black());
        EndFrame();
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(TextTests, AddingFontsFromMemoryAndFiles)
    {
        // The embedded italic face doubles as test data for a host-provided font.
        const std::span<const uint8_t> data = GetEmbeddedFont(EmbeddedFont::PublicSansItalic);
        Font* custom = AddFontFromMemory(data, {.Name = "Custom"});
        ASSERT_NE(custom, nullptr);
        EXPECT_NE(custom, GetDefaultFont());

        TextSpec spec;
        spec.Font = custom;
        EXPECT_GT(GetWidth("Custom font", spec), 0.0f);

        // Garbage is rejected with a log message instead of a crash.
        m_Logs.clear();
        const uint8_t garbage[64] = {1, 2, 3, 4};
        EXPECT_EQ(AddFontFromMemory(garbage, {.Name = "Garbage"}), nullptr);
        ASSERT_FALSE(m_Logs.empty());
        EXPECT_EQ(m_Logs.back().Level, LogLevel::Error);
        EXPECT_EQ(m_Logs.back().Source, "Text");

        EXPECT_EQ(AddFontFromFile("this/file/does/not/exist.ttf"), nullptr);
    }

    TEST_F(TextTests, WrapsBetweenWords)
    {
        TextSpec spec = GetTextSpec(TextStyle::Body);
        const std::string_view text = "one two three four five six seven eight nine ten";
        const float fullWidth = GetWidth(text, spec);

        spec.MaxWidth = 100.0f;
        const Vec2 wrapped = MeasureText(text, spec);
        EXPECT_LE(wrapped.X, 100.0f);
        EXPECT_GT(wrapped.X, 50.0f); // lines are filled reasonably
        const float lines = wrapped.Y / 16.0f;
        EXPECT_FLOAT_EQ(lines, std::round(lines));
        EXPECT_GE(lines, std::ceil(fullWidth / 100.0f));
        EXPECT_LE(lines, std::ceil(fullWidth / 100.0f) + 2.0f);

        // A wide limit changes nothing.
        spec.MaxWidth = fullWidth + 1.0f;
        EXPECT_FLOAT_EQ(MeasureText(text, spec).Y, 16.0f);

        // Explicit line breaks still count.
        spec.MaxWidth = 1000.0f;
        EXPECT_FLOAT_EQ(MeasureText("first\nsecond", spec).Y, 32.0f);
    }

    TEST_F(TextTests, WordsLongerThanTheLineBreakAnywhere)
    {
        TextSpec spec = GetTextSpec(TextStyle::Body);
        spec.MaxWidth = 60.0f;
        const Vec2 size = MeasureText("Donaudampfschifffahrtsgesellschaft", spec);
        EXPECT_LE(size.X, 60.0f);
        EXPECT_GE(size.Y, 3.0f * 16.0f);
    }

    TEST_F(TextTests, WrappedTextIsDrawnInsideItsWidth)
    {
        TextSpec spec = GetTextSpec(TextStyle::Body);
        spec.MaxWidth = 120.0f;
        NewFrame();
        GetDrawList().AddText(Vec2(40.0f, 20.0f), "The quick brown fox jumps over the lazy dog", spec, Color::Black());
        const std::span<const DrawVertex> vertices = GetDrawList().GetVertices();
        ASSERT_FALSE(vertices.empty());
        float lowest = 0.0f;
        for (const DrawVertex& vertex : vertices)
        {
            EXPECT_GE(vertex.Position.X, 39.0f);
            EXPECT_LE(vertex.Position.X, 161.0f);
            lowest = std::max(lowest, vertex.Position.Y);
        }
        EXPECT_GT(lowest, 20.0f + 32.0f); // at least three lines
        EndFrame();
    }

    TEST_F(TextTests, TruncatesWithAnEllipsis)
    {
        TextSpec spec = GetTextSpec(TextStyle::Body);
        const std::string_view text = "A label that is far too long for its column";
        spec.MaxWidth = 90.0f;
        spec.Wraps = false;
        const Vec2 size = MeasureText(text, spec);
        EXPECT_FLOAT_EQ(size.X, 90.0f);
        EXPECT_FLOAT_EQ(size.Y, 16.0f);

        NewFrame();
        GetDrawList().AddText(Vec2(10.0f, 10.0f), text, spec, Color::Black());
        const std::span<const DrawVertex> vertices = GetDrawList().GetVertices();
        ASSERT_GE(vertices.size(), 8u);
        for (const DrawVertex& vertex : vertices)
            EXPECT_LE(vertex.Position.X, 101.0f);
        // Fewer glyphs than the full text, and the last one is the ellipsis.
        EXPECT_LT(vertices.size() / 4, text.size());
        const Internal::ShapedLine& ellipsis = GetTextSystem().Shape("\xE2\x80\xA6", GetTextSpec(TextStyle::Body));
        ASSERT_EQ(ellipsis.Glyphs.size(), 1u);
        EndFrame();

        // Text that fits is left alone.
        spec.MaxWidth = 1000.0f;
        EXPECT_FLOAT_EQ(MeasureText(text, spec).X, GetWidth(text, GetTextSpec(TextStyle::Body)));
    }

    TEST_F(TextTests, AlignmentPlacesLinesInsideTheWidth)
    {
        TextSpec spec = GetTextSpec(TextStyle::Body);
        const float width = GetWidth("Hi", spec);
        spec.MaxWidth = 200.0f;

        const auto firstGlyphX = [&](TextAlignment alignment)
        {
            spec.Alignment = alignment;
            NewFrame();
            GetDrawList().AddText(Vec2(0.0f, 0.0f), "Hi", spec, Color::Black());
            const float x = GetDrawList().GetVertices()[0].Position.X;
            EndFrame();
            return x;
        };
        const float leading = firstGlyphX(TextAlignment::Leading);
        const float center = firstGlyphX(TextAlignment::Center);
        const float trailing = firstGlyphX(TextAlignment::Trailing);
        EXPECT_NEAR(center - leading, (200.0f - width) * 0.5f, 1.0f);
        EXPECT_NEAR(trailing - leading, 200.0f - width, 1.0f);
    }

    TEST_F(TextTests, CaretPositionsFollowTheGlyphs)
    {
        const TextSpec spec = GetTextSpec(TextStyle::Body);
        const std::string text = "Wi\xC3\xA4\xE2\x82\xACm"; // W, i, ä (2 bytes), € (3 bytes), m
        std::vector<float> positions;
        GetCaretPositions(text, spec, positions);
        ASSERT_EQ(positions.size(), text.size() + 1);

        EXPECT_FLOAT_EQ(positions[0], 0.0f);
        EXPECT_FLOAT_EQ(positions.back(), GetWidth(text, spec));
        // Never decreasing, and strictly increasing from one character to the next.
        for (size_t i = 1; i < positions.size(); i++)
            EXPECT_GE(positions[i], positions[i - 1]);
        EXPECT_GT(positions[1], positions[0]);
        EXPECT_GT(positions[2], positions[1]);
        EXPECT_GT(positions[4], positions[2]);
        EXPECT_GT(positions[7], positions[4]);
        // "W" is wider than "i".
        EXPECT_GT(positions[1] - positions[0], positions[2] - positions[1]);
        // A prefix ends where its caret position is.
        EXPECT_NEAR(positions[2], GetWidth("Wi", spec), 0.5f);

        // An empty line has a single caret position.
        GetCaretPositions("", spec, positions);
        ASSERT_EQ(positions.size(), 1u);
        EXPECT_FLOAT_EQ(positions[0], 0.0f);
    }
} // namespace Carbon
