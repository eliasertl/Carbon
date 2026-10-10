#include "Support/ContextTest.h"

#include <cmath>
#include <filesystem>
#include <string>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/UTF8.h"
#include "Carbon/Text/Internal/TextSystem.h"
#include "Support/TestFonts.h"

namespace Carbon
{
    // Color glyphs: COLR glyphs painted in their palette's colors and the PNG images of CBDT and sbix fonts, in an
    // atlas of their own, drawn without the text color. The test fonts are built in memory (Support/TestFonts).
    class ColorGlyphTests : public ContextTest
    {
    protected:
        Internal::TextSystem& GetTextSystem() { return *Internal::GetContext().Text; }

        static std::string ToUTF8(char32_t codepoint)
        {
            std::string text;
            AppendUTF8(text, codepoint);
            return text;
        }

        // Draws `text` in its own frame and returns the frame's quads of color glyphs.
        std::vector<Rect> DrawText(std::string_view text, float size, Color color = Color::Black())
        {
            NewFrame();
            TextSpec spec;
            spec.Size = size;
            spec.Font = m_Font;
            GetDrawList().AddText(Vec2(10.0f, 10.0f), text, spec, color);
            EndFrame();

            std::vector<Rect> quads;
            const DrawData& drawData = GetDrawData();
            for (size_t vertex = 0; vertex + 3 < drawData.Vertices.size(); vertex += 4)
            {
                if (drawData.Primitives[drawData.Vertices[vertex].Primitive].Kind == DrawPrimitiveKind::ColorGlyph)
                    quads.push_back(
                        Rect::FromMinMax(drawData.Vertices[vertex].Position, drawData.Vertices[vertex + 2].Position));
            }
            return quads;
        }

        // The RGBA texel of the color atlas at a point of the first color glyph quad, given as a fraction of it.
        std::array<uint8_t, 4> SampleColorGlyph(float u, float v)
        {
            const DrawData& drawData = GetDrawData();
            for (size_t vertex = 0; vertex + 3 < drawData.Vertices.size(); vertex += 4)
            {
                if (drawData.Primitives[drawData.Vertices[vertex].Primitive].Kind != DrawPrimitiveKind::ColorGlyph)
                    continue;
                const Vec2 min = drawData.Vertices[vertex].UV;
                const Vec2 max = drawData.Vertices[vertex + 2].UV;
                const Internal::GlyphAtlas& atlas = *GetTextSystem().GetColorAtlas();
                const uint32_t x = static_cast<uint32_t>(min.X + (max.X - min.X) * u);
                const uint32_t y = static_cast<uint32_t>(min.Y + (max.Y - min.Y) * v);
                const uint8_t* texel = atlas.GetPixels().data() + (static_cast<size_t>(y) * atlas.GetWidth() + x) * 4;
                return {texel[0], texel[1], texel[2], texel[3]};
            }
            ADD_FAILURE() << "no color glyph was drawn";
            return {};
        }

        static std::array<uint8_t, 4> Opaque(uint32_t rgb)
        {
            return {static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>((rgb >> 8) & 0xFF),
                    static_cast<uint8_t>(rgb & 0xFF), 255};
        }

        Font* m_Font = nullptr;
        std::vector<uint8_t> m_FontData;
    };

    TEST_F(ColorGlyphTests, TextWithoutColorGlyphsMakesNoColorAtlas)
    {
        NewFrame();
        GetDrawList().AddText(Vec2(10.0f, 10.0f), "Plain text", TextSpec(), Color::Black());
        EndFrame();
        EXPECT_EQ(GetTextSystem().GetColorAtlas(), nullptr);
        for (const DrawCommand& command : GetDrawData().Commands)
            EXPECT_NE(command.Texture, ColorGlyphAtlasTextureID);
    }

    TEST_F(ColorGlyphTests, ColrGlyphsArePaintedInTheirPalettesColors)
    {
#if !defined(CARBON_TESTS_HAVE_COLOR_PAINT)
        GTEST_SKIP() << "built without HarfBuzz's raster library, which paints COLR glyphs";
#endif
        m_Font = AddFontFromMemory(TestFonts::MakeColrFont(), {.Name = "Colr"});
        ASSERT_NE(m_Font, nullptr);
        GetIO().SetContentScale(2.0f);
        const std::vector<Rect> quads = DrawText(ToUTF8(TestFonts::ColorCodepoint), 20.0f);

        // One quad of a whole em (20 points), in a command that samples the color glyph atlas.
        ASSERT_EQ(quads.size(), 1u);
        EXPECT_NEAR(quads[0].Width, 20.0f, 1.0f);
        EXPECT_NEAR(quads[0].Height, 20.0f, 1.0f);
        ASSERT_NE(GetTextSystem().GetColorAtlas(), nullptr);
        EXPECT_EQ(GetTextSystem().GetColorAtlas()->GetBytesPerTexel(), 4u);
        bool hasColorCommand = false;
        for (const DrawCommand& command : GetDrawData().Commands)
            hasColorCommand = hasColorCommand || command.Texture == ColorGlyphAtlasTextureID;
        EXPECT_TRUE(hasColorCommand);

        // The inner layer on top in the top-left quarter, the outer one everywhere else: upright, not mirrored.
        EXPECT_EQ(SampleColorGlyph(0.3f, 0.3f), Opaque(TestFonts::InnerColor));
        EXPECT_EQ(SampleColorGlyph(0.7f, 0.3f), Opaque(TestFonts::OuterColor));
        EXPECT_EQ(SampleColorGlyph(0.3f, 0.7f), Opaque(TestFonts::OuterColor));
        EXPECT_EQ(SampleColorGlyph(0.7f, 0.7f), Opaque(TestFonts::OuterColor));
        EXPECT_EQ(SampleColorGlyph(0.03f, 0.03f), Opaque(TestFonts::OuterColor));
    }

    TEST_F(ColorGlyphTests, TheTextColorDoesNotTintColorGlyphsButItsOpacityFadesThem)
    {
        m_FontData = TestFonts::MakeCbdtFont();
        m_Font = AddFontFromMemory(m_FontData, {.Name = "Cbdt"});
        ASSERT_NE(m_Font, nullptr);
        DrawText(ToUTF8(TestFonts::ColorCodepoint), 20.0f, Color(1.0f, 0.0f, 0.0f, 0.5f));
        const DrawData& drawData = GetDrawData();
        ASSERT_EQ(drawData.Vertices.size(), 4u);
        // White, half transparent: the shader multiplies the glyph's colors by it.
        EXPECT_EQ(drawData.Vertices[0].Color, Color(1.0f, 1.0f, 1.0f, 0.5f).ToRGBA8());
    }

    TEST_F(ColorGlyphTests, AGlyphWithoutColorInAColorFontIsDrawnInTheTextColor)
    {
        m_Font = AddFontFromMemory(TestFonts::MakeColrFont(), {.Name = "Colr"});
        ASSERT_NE(m_Font, nullptr);
        const std::vector<Rect> quads = DrawText(ToUTF8(TestFonts::PlainCodepoint), 20.0f, Color::Black());
        EXPECT_TRUE(quads.empty());
        const DrawData& drawData = GetDrawData();
        ASSERT_EQ(drawData.Vertices.size(), 4u);
        EXPECT_EQ(drawData.Primitives[drawData.Vertices[0].Primitive].Kind, DrawPrimitiveKind::Glyph);
        EXPECT_EQ(drawData.Vertices[0].Color, Color::Black().ToRGBA8());
    }

    TEST_F(ColorGlyphTests, CbdtImagesAreScaledFromTheirStrikeToEveryScale)
    {
        m_FontData = TestFonts::MakeCbdtFont(64);
        m_Font = AddFontFromMemory(m_FontData, {.Name = "Cbdt"});
        ASSERT_NE(m_Font, nullptr) << "a font of bitmaps only is accepted for its color images";

        for (const float scale : {1.0f, 1.5f, 2.0f, 3.0f})
        {
            GetIO().SetContentScale(scale);
            const std::vector<Rect> quads = DrawText(ToUTF8(TestFonts::ColorCodepoint), 20.0f);
            ASSERT_EQ(quads.size(), 1u) << "scale " << scale;
            // An em is 20 points at every scale; the image is scaled to it from its 64-pixel strike.
            EXPECT_NEAR(quads[0].Width, 20.0f, 1.0f / scale) << "scale " << scale;
            EXPECT_NEAR(quads[0].Height, 20.0f, 1.0f / scale) << "scale " << scale;
            // Upright: the top half above the bottom half.
            EXPECT_EQ(SampleColorGlyph(0.5f, 0.25f), Opaque(TestFonts::OuterColor)) << "scale " << scale;
            EXPECT_EQ(SampleColorGlyph(0.5f, 0.75f), Opaque(TestFonts::InnerColor)) << "scale " << scale;
            const Internal::GlyphAtlas& atlas = *GetTextSystem().GetColorAtlas();
            EXPECT_EQ(GetDrawData().Vertices[2].UV.X - GetDrawData().Vertices[0].UV.X, std::round(20.0f * scale));
            EXPECT_LE(GetDrawData().Vertices[2].UV.X, static_cast<float>(atlas.GetWidth()));
        }
    }

    TEST_F(ColorGlyphTests, ColorGlyphsSitOnTheBaseline)
    {
        m_FontData = TestFonts::MakeCbdtFont(64);
        m_Font = AddFontFromMemory(m_FontData, {.Name = "Cbdt"});
        TextSpec spec;
        spec.Size = 20.0f;
        spec.Font = m_Font;
        for (const float scale : {1.0f, 2.0f})
        {
            GetIO().SetContentScale(scale);
            const std::vector<Rect> quads = DrawText(ToUTF8(TestFonts::ColorCodepoint), 20.0f);
            ASSERT_EQ(quads.size(), 1u);
            // The image stands on the baseline, as the font's metrics say.
            const float baseline = 10.0f + GetFontMetrics(spec).Baseline;
            EXPECT_NEAR(quads[0].GetBottom(), baseline, 1.0f / scale) << "scale " << scale;
        }
    }

    TEST_F(ColorGlyphTests, SbixImagesAreScaledUprightOntoTheBaseline)
    {
        m_FontData = TestFonts::MakeSbixFont(64);
        m_Font = AddFontFromMemory(m_FontData, {.Name = "Sbix"});
        ASSERT_NE(m_Font, nullptr) << "a font of sbix bitmaps only is accepted for its color images";
        TextSpec spec;
        spec.Size = 20.0f;
        spec.Font = m_Font;
        for (const float scale : {1.0f, 2.0f})
        {
            GetIO().SetContentScale(scale);
            const std::vector<Rect> quads = DrawText(ToUTF8(TestFonts::ColorCodepoint), 20.0f);
            ASSERT_EQ(quads.size(), 1u) << "scale " << scale;
            // The 64-pixel image is scaled to an em of 20 points, upright, and stands on the baseline.
            EXPECT_NEAR(quads[0].Width, 20.0f, 1.0f / scale) << "scale " << scale;
            EXPECT_NEAR(quads[0].Height, 20.0f, 1.0f / scale) << "scale " << scale;
            EXPECT_NEAR(quads[0].GetBottom(), 10.0f + GetFontMetrics(spec).Baseline, 1.0f / scale) << "scale " << scale;
            EXPECT_EQ(SampleColorGlyph(0.5f, 0.25f), Opaque(TestFonts::OuterColor)) << "scale " << scale;
            EXPECT_EQ(SampleColorGlyph(0.5f, 0.75f), Opaque(TestFonts::InnerColor)) << "scale " << scale;
        }
    }

    TEST_F(ColorGlyphTests, AColorGlyphIsRasterizedOncePerSize)
    {
        m_FontData = TestFonts::MakeCbdtFont();
        m_Font = AddFontFromMemory(m_FontData, {.Name = "Cbdt"});
        // Four positions with different fractions of a pixel: a coverage glyph would take four sub-pixel bins.
        NewFrame();
        TextSpec spec;
        spec.Size = 20.0f;
        spec.Font = m_Font;
        for (int i = 0; i < 4; i++)
            GetDrawList().AddText(Vec2(10.0f + 0.25f * static_cast<float>(i), 10.0f + 30.0f * static_cast<float>(i)),
                                  ToUTF8(TestFonts::ColorCodepoint), spec, Color::Black());
        EndFrame();
        EXPECT_EQ(GetTextSystem().GetCachedGlyphCount(), 1u);
    }

    TEST_F(ColorGlyphTests, ANewContentScaleEmptiesTheColorAtlasToo)
    {
        m_FontData = TestFonts::MakeCbdtFont();
        m_Font = AddFontFromMemory(m_FontData, {.Name = "Cbdt"});
        DrawText(ToUTF8(TestFonts::ColorCodepoint), 20.0f);
        const uint32_t generation = GetTextSystem().GetColorAtlas()->GetGeneration();
        GetIO().SetContentScale(2.0f);
        DrawText(ToUTF8(TestFonts::ColorCodepoint), 20.0f);
        EXPECT_NE(GetTextSystem().GetColorAtlas()->GetGeneration(), generation);
        EXPECT_EQ(GetTextSystem().GetCachedGlyphCount(), 1u);
    }

    // ---- Fallback: which font draws an emoji --------------------------------------------------------------------

    class EmojiFallbackTests : public ColorGlyphTests
    {
    protected:
        // A plain font first, then a color font: the order in which an application would add a CJK font and the
        // system's emoji font.
        void SetUp() override
        {
            ColorGlyphTests::SetUp();
            m_PlainData = TestFonts::MakePlainFont();
            m_ColorData = TestFonts::MakeCbdtFont();
            m_Plain = AddFontFromMemory(m_PlainData, {.Name = "Plain"});
            m_Color = AddFontFromMemory(m_ColorData, {.Name = "Color"});
            ASSERT_NE(m_Plain, nullptr);
            ASSERT_NE(m_Color, nullptr);
        }

        // The faces the glyphs of `text` come from, in the default font.
        std::vector<uint16_t> GetFaces(std::string_view text)
        {
            std::vector<uint16_t> faces;
            for (const Internal::ShapedGlyph& glyph : GetTextSystem().Shape(text, TextSpec()).Glyphs)
                faces.push_back(glyph.Face);
            return faces;
        }

        uint16_t GetFace(Font* font) const { return font->Roman; }

        std::vector<uint8_t> m_PlainData;
        std::vector<uint8_t> m_ColorData;
        Font* m_Plain = nullptr;
        Font* m_Color = nullptr;
    };

    TEST_F(EmojiFallbackTests, AnEmojiPrefersAColorFontAddedAfterAPlainOne)
    {
        // The plain font comes first in the order of fallbacks and has a glyph, but an emoji is shown as emoji.
        EXPECT_EQ(GetFaces(ToUTF8(TestFonts::ColorCodepoint)), std::vector<uint16_t>{GetFace(m_Color)});
        // A character that is not an emoji still takes the first font that has it.
        EXPECT_EQ(GetFaces(ToUTF8(TestFonts::PlainCodepoint)), std::vector<uint16_t>{GetDefaultFont()->Roman});
    }

    TEST_F(EmojiFallbackTests, TextPresentationPrefersAFontWithoutColor)
    {
        // VS15 asks for the plain version, which the plain font has.
        const std::string text = ToUTF8(TestFonts::ColorCodepoint) + ToUTF8(0xFE0E);
        const std::vector<uint16_t> faces = GetFaces(text);
        ASSERT_FALSE(faces.empty());
        EXPECT_EQ(faces[0], GetFace(m_Plain));
    }

    TEST_F(EmojiFallbackTests, ASequenceStaysInTheFontOfItsFirstCharacter)
    {
        // A zero-width joiner the plain font has too, a skin tone and VS16: none of them breaks the run, so the
        // color font gets the whole sequence and could shape it into one glyph.
        const std::string family = ToUTF8(TestFonts::ColorCodepoint) + ToUTF8(TestFonts::ZwjCodepoint) +
                                   ToUTF8(TestFonts::SecondColorCodepoint) + ToUTF8(0x1F3FD) + ToUTF8(0xFE0F);
        for (const uint16_t face : GetFaces(family))
            EXPECT_EQ(face, GetFace(m_Color));

        // Text around the sequence keeps its own font.
        const std::vector<uint16_t> faces = GetFaces("a" + family + "b");
        ASSERT_GE(faces.size(), 3u);
        EXPECT_EQ(faces.front(), GetDefaultFont()->Roman);
        EXPECT_EQ(faces.back(), GetDefaultFont()->Roman);
    }

    TEST_F(EmojiFallbackTests, TwoRegionalIndicatorsAreOneFlagAndAThirdStartsTheNext)
    {
        // No font here has the letters of flags; what matters is where the clusters end.
        const std::string flags = ToUTF8(0x1F1E9) + ToUTF8(0x1F1EA) + ToUTF8(0x1F1EB);
        const Internal::ShapedLine& shaped = GetTextSystem().Shape(flags, TextSpec());
        EXPECT_FALSE(shaped.Glyphs.empty());
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(EmojiFallbackTests, TextWithoutEmojiShapesAsBefore)
    {
        // Latin text and a joiner between letters (used by some scripts) stay in the default font.
        const std::string text = "Hello" + ToUTF8(TestFonts::ZwjCodepoint) + "World";
        const uint16_t defaultFace = GetDefaultFont()->Roman;
        for (const uint16_t face : GetFaces(text))
            EXPECT_EQ(face, defaultFace);
    }

    // The system's emoji font, where there is one: each emoji sequence becomes one emoji, a single color glyph or
    // (Segoe UI Emoji's families) a few overlapping ones, one emoji wide.
    TEST_F(ColorGlyphTests, TheSystemsEmojiFontJoinsSequencesIntoOneEmoji)
    {
        const char* candidates[] = {
            "C:/Windows/Fonts/seguiemj.ttf", "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
            "/usr/share/fonts/noto/NotoColorEmoji.ttf", "/System/Library/Fonts/Apple Color Emoji.ttc"};
        std::filesystem::path path;
        std::error_code error;
        for (const char* candidate : candidates)
        {
            if (std::filesystem::is_regular_file(candidate, error))
            {
                path = candidate;
                break;
            }
        }
        if (path.empty())
            GTEST_SKIP() << "no system emoji font";
        m_Font = AddFontFromFile(path);
        ASSERT_NE(m_Font, nullptr) << path.string();

        const std::string sequences[] = {
            ToUTF8(0x1F600),                                                                       // grinning face
            ToUTF8(0x1F44D) + ToUTF8(0x1F3FD),                                                     // thumbs up, tone
            ToUTF8(0x1F468) + ToUTF8(0x200D) + ToUTF8(0x1F469) + ToUTF8(0x200D) + ToUTF8(0x1F467), // family
            ToUTF8(0x2764) + ToUTF8(0xFE0F),                                                       // red heart
            "1" + ToUTF8(0xFE0F) + ToUTF8(0x20E3),                                                 // keycap one
        };
        const float emojiWidth = GetTextSystem().Shape(sequences[0], TextSpec()).Width;
        for (const std::string& sequence : sequences)
        {
            // Shaped from the default font: the emoji font is a fallback.
            const Internal::ShapedLine& shaped = GetTextSystem().Shape(sequence, TextSpec());
            ASSERT_FALSE(shaped.Glyphs.empty());
            for (const Internal::ShapedGlyph& glyph : shaped.Glyphs)
            {
                EXPECT_EQ(glyph.Cluster, 0u) << "sequence of " << sequence.size() << " bytes";
                EXPECT_TRUE(glyph.IsColor) << "sequence of " << sequence.size() << " bytes";
            }
            EXPECT_NEAR(shaped.Width, emojiWidth, emojiWidth * 0.2f) << "sequence of " << sequence.size() << " bytes";
        }
        for (const float scale : {1.0f, 2.0f})
        {
            GetIO().SetContentScale(scale);
            m_Font = nullptr;
            EXPECT_FALSE(DrawText(sequences[2], 20.0f).empty()) << "scale " << scale;
        }
    }

    TEST_F(ColorGlyphTests, ColorGlyphsAreMarkedWhenTheyAreShaped)
    {
        m_FontData = TestFonts::MakeCbdtFont();
        m_Font = AddFontFromMemory(m_FontData, {.Name = "Cbdt"});
        // "A" from the default font, the emoji from the color font as a fallback, "B" again.
        const std::string text = "A" + ToUTF8(TestFonts::ColorCodepoint) + "B";
        const Internal::ShapedLine& shaped = GetTextSystem().Shape(text, TextSpec());
        ASSERT_EQ(shaped.Glyphs.size(), 3u);
        EXPECT_FALSE(shaped.Glyphs[0].IsColor);
        EXPECT_TRUE(shaped.Glyphs[1].IsColor);
        EXPECT_FALSE(shaped.Glyphs[2].IsColor);
        EXPECT_NE(shaped.Glyphs[1].Face, shaped.Glyphs[0].Face);

        // Drawn, the emoji breaks the command into three: the atlas, the color atlas, the atlas.
        NewFrame();
        GetDrawList().AddText(Vec2(10.0f, 10.0f), text, TextSpec(), Color::Black());
        EndFrame();
        const std::span<const DrawCommand> commands = GetDrawData().Commands;
        ASSERT_EQ(commands.size(), 3u);
        EXPECT_EQ(commands[0].Texture, TextureID());
        EXPECT_EQ(commands[1].Texture, ColorGlyphAtlasTextureID);
        EXPECT_EQ(commands[2].Texture, TextureID());
    }
} // namespace Carbon
