#include <gtest/gtest.h>

#include <cstring>
#include <set>
#include <string>
#include <string_view>

#include "Carbon/Carbon.h"

namespace Carbon
{
    TEST(EmbeddedAssetsTests, EveryFontIsPresentAndLooksLikeTrueType)
    {
        for (int i = 0; i < static_cast<int>(EmbeddedFont::Count); i++)
        {
            const std::span<const uint8_t> data = GetEmbeddedFont(static_cast<EmbeddedFont>(i));
            ASSERT_GT(data.size(), 50u * 1024u) << "font " << i;
            // TrueType files start with the version 0x00010000.
            EXPECT_EQ(data[0], 0x00);
            EXPECT_EQ(data[1], 0x01);
            EXPECT_EQ(data[2], 0x00);
            EXPECT_EQ(data[3], 0x00);
        }
        EXPECT_TRUE(GetEmbeddedFont(EmbeddedFont::Count).empty());
    }

    TEST(EmbeddedAssetsTests, FontsAreDistinctFiles)
    {
        const std::span<const uint8_t> roman = GetEmbeddedFont(EmbeddedFont::PublicSansRoman);
        const std::span<const uint8_t> italic = GetEmbeddedFont(EmbeddedFont::PublicSansItalic);
        const std::span<const uint8_t> regular = GetEmbeddedFont(EmbeddedFont::PhosphorRegular);
        const std::span<const uint8_t> bold = GetEmbeddedFont(EmbeddedFont::PhosphorBold);
        const std::span<const uint8_t> fill = GetEmbeddedFont(EmbeddedFont::PhosphorFill);
        EXPECT_NE(roman.data(), italic.data());
        EXPECT_NE(roman.size(), italic.size());
        EXPECT_NE(regular.size(), bold.size());
        EXPECT_NE(bold.size(), fill.size());
    }

    TEST(EmbeddedAssetsTests, IconConstantsAreSingleCodepointsInThePrivateUseArea)
    {
        EXPECT_EQ(Icons::Count, 1530u);
        std::set<std::string> names;
        for (const Icons::Entry& entry : Icons::All)
        {
            const std::string_view glyph = entry.Glyph;
            const UTF8Decoded decoded = DecodeUTF8(glyph, 0);
            EXPECT_EQ(decoded.Length, glyph.size()) << entry.Name;
            EXPECT_GE(decoded.Codepoint, char32_t(0xE000)) << entry.Name;
            EXPECT_LE(decoded.Codepoint, char32_t(0xF8FF)) << entry.Name;
            EXPECT_TRUE(names.insert(entry.Name).second) << "duplicate name " << entry.Name;
        }
        EXPECT_EQ(names.size(), Icons::Count);
    }

    TEST(EmbeddedAssetsTests, WellKnownIconsHaveTheirPhosphorCodepoints)
    {
        EXPECT_EQ(DecodeUTF8(Icons::House, 0).Codepoint, char32_t(0xE2C2));
        EXPECT_EQ(DecodeUTF8(Icons::X, 0).Codepoint, char32_t(0xE4F6));
        EXPECT_EQ(DecodeUTF8(Icons::Plus, 0).Codepoint, char32_t(0xE3D4));
        EXPECT_EQ(DecodeUTF8(Icons::Gear, 0).Codepoint, char32_t(0xE270));
        EXPECT_EQ(DecodeUTF8(Icons::MagnifyingGlass, 0).Codepoint, char32_t(0xE30C));
        EXPECT_STREQ(Icons::All[0].Name, "Acorn");
        EXPECT_EQ(std::strcmp(Icons::All[0].Glyph, Icons::Acorn), 0);
    }
} // namespace Carbon
