#include <cmath>
#include <string>
#include <vector>

#include "Carbon/Core/UTF8.h"
#include "Support/BackendTest.h"
#include "Support/TestFonts.h"

namespace Carbon
{
    // Color glyphs on every backend: the color atlas's format and the shader path that draws it in its own colors.
    // The test fonts are built in memory (Support/TestFonts); their glyphs are an em square, CBDT's red above blue,
    // COLR's red with a blue top-left quarter.
    class BackendColorGlyphTests : public BackendTest
    {
    protected:
        static constexpr float Size = 40.0f;
        static constexpr float Left = 10.0f;
        static constexpr float Top = 10.0f;

        static std::string Emoji()
        {
            std::string text;
            AppendUTF8(text, TestFonts::ColorCodepoint);
            return text;
        }

        // Draws a line with the emoji at (Left, Top), and text in the same color after it, which samples the
        // glyph atlas in the same frame.
        RenderedImage Draw(Font* font, float scale, Color textColor, Color background,
                           TextureFormat format = TextureFormat::RGBA8Unorm)
        {
            if (format != TextureFormat::RGBA8Unorm)
            {
                m_Harness->ShutdownBackend();
                EXPECT_TRUE(m_Harness->InitBackend(format));
            }
            TextSpec spec;
            spec.Font = font;
            spec.Size = Size;
            m_Baseline = Top + GetFontMetrics(spec).Baseline;
            return RenderFrame(120.0f, 70.0f, scale, background,
                               [&](DrawList& drawList)
                               {
                                   drawList.AddText(Vec2(Left, Top), Emoji() + "x", spec, textColor);
                                   // A shape after the text joins the last command, whichever atlas it samples.
                                   drawList.AddRect(Rect(100.0f, 60.0f, 10.0f, 5.0f), textColor);
                               });
        }

        // The pixel at a point of the em square of the glyph, given as fractions of it from its top-left corner.
        static Color Sample(const RenderedImage& image, float scale, float u, float v, float baseline)
        {
            const float x = (Left + Size * u) * scale;
            const float y = (baseline - Size * (1.0f - v)) * scale;
            return image.GetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
        }

        static void ExpectColor(Color actual, uint32_t rgb, std::string_view where)
        {
            const Color expected = Color::FromHex(rgb);
            EXPECT_NEAR(actual.R, expected.R, 3.0f / 255.0f) << where;
            EXPECT_NEAR(actual.G, expected.G, 3.0f / 255.0f) << where;
            EXPECT_NEAR(actual.B, expected.B, 3.0f / 255.0f) << where;
        }

        float m_Baseline = 0.0f;
    };

    TEST_P(BackendColorGlyphTests, CbdtGlyphsKeepTheirColorsAtEveryScale)
    {
        const std::vector<uint8_t> data = TestFonts::MakeCbdtFont(64);
        Font* font = AddFontFromMemory(data, {.Name = "Cbdt"});
        ASSERT_NE(font, nullptr);
        for (const float scale : {1.0f, 1.5f, 2.0f})
        {
            // Green text: the glyph keeps its own colors, upright.
            const RenderedImage image = Draw(font, scale, Color::FromHex(0x00C000), Color::White());
            ExpectColor(Sample(image, scale, 0.5f, 0.2f, m_Baseline), TestFonts::OuterColor, "top half");
            ExpectColor(Sample(image, scale, 0.5f, 0.8f, m_Baseline), TestFonts::InnerColor, "bottom half");
        }
    }

    TEST_P(BackendColorGlyphTests, ColrGlyphsKeepTheirColorsAtEveryScale)
    {
#if !defined(CARBON_TESTS_HAVE_COLOR_PAINT)
        GTEST_SKIP() << "built without HarfBuzz's raster library, which paints COLR glyphs";
#endif
        const std::vector<uint8_t> data = TestFonts::MakeColrFont();
        Font* font = AddFontFromMemory(data, {.Name = "Colr"});
        ASSERT_NE(font, nullptr);
        for (const float scale : {1.0f, 1.5f, 2.0f})
        {
            const RenderedImage image = Draw(font, scale, Color::FromHex(0x00C000), Color::White());
            ExpectColor(Sample(image, scale, 0.3f, 0.3f, m_Baseline), TestFonts::InnerColor, "top left");
            ExpectColor(Sample(image, scale, 0.7f, 0.3f, m_Baseline), TestFonts::OuterColor, "top right");
            ExpectColor(Sample(image, scale, 0.3f, 0.7f, m_Baseline), TestFonts::OuterColor, "bottom left");
            ExpectColor(Sample(image, scale, 0.7f, 0.7f, m_Baseline), TestFonts::OuterColor, "bottom right");
        }
    }

    TEST_P(BackendColorGlyphTests, TheTextsOpacityFadesColorGlyphs)
    {
        const std::vector<uint8_t> data = TestFonts::MakeCbdtFont(64);
        Font* font = AddFontFromMemory(data, {.Name = "Cbdt"});
        // Half transparent over black: half the glyph's color.
        const RenderedImage image = Draw(font, 1.0f, Color(0.0f, 0.75f, 0.0f, 0.5f), Color::Black());
        const Color pixel = Sample(image, 1.0f, 0.5f, 0.2f, m_Baseline);
        const Color expected = Color::FromHex(TestFonts::OuterColor);
        EXPECT_NEAR(pixel.R, expected.R * 0.5f, 3.0f / 255.0f);
        EXPECT_NEAR(pixel.G, expected.G * 0.5f, 3.0f / 255.0f);
        EXPECT_NEAR(pixel.B, expected.B * 0.5f, 3.0f / 255.0f);
    }

    TEST_P(BackendColorGlyphTests, SrgbTargetsGetTheSameColors)
    {
        const std::vector<uint8_t> data = TestFonts::MakeCbdtFont(64);
        Font* font = AddFontFromMemory(data, {.Name = "Cbdt"});
        // The atlas holds sRGB values; written into an sRGB target they read back as the same values.
        const RenderedImage image = Draw(font, 1.0f, Color::Black(), Color::White(), TextureFormat::RGBA8UnormSrgb);
        ExpectColor(Sample(image, 1.0f, 0.5f, 0.2f, m_Baseline), TestFonts::OuterColor, "top half");
        ExpectColor(Sample(image, 1.0f, 0.5f, 0.8f, m_Baseline), TestFonts::InnerColor, "bottom half");
    }

    CB_INSTANTIATE_BACKEND_TESTS(BackendColorGlyphTests);
} // namespace Carbon
