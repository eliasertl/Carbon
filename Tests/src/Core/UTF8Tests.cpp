#include <gtest/gtest.h>

#include <string>

#include "Carbon/Carbon.h"

namespace Carbon
{
    TEST(UTF8Tests, DecodesEverySequenceLength)
    {
        const std::string text = "a\xC3\xA4\xE2\x82\xAC\xF0\x9F\x98\x80"; // a, ä, €, 😀
        UTF8Decoded decoded = DecodeUTF8(text, 0);
        EXPECT_EQ(decoded.Codepoint, U'a');
        EXPECT_EQ(decoded.Length, 1u);
        decoded = DecodeUTF8(text, 1);
        EXPECT_EQ(decoded.Codepoint, char32_t(0xE4));
        EXPECT_EQ(decoded.Length, 2u);
        decoded = DecodeUTF8(text, 3);
        EXPECT_EQ(decoded.Codepoint, char32_t(0x20AC));
        EXPECT_EQ(decoded.Length, 3u);
        decoded = DecodeUTF8(text, 6);
        EXPECT_EQ(decoded.Codepoint, char32_t(0x1F600));
        EXPECT_EQ(decoded.Length, 4u);
        EXPECT_EQ(DecodeUTF8(text, text.size()).Length, 0u);
        EXPECT_EQ(CountCodepoints(text), 4u);
    }

    TEST(UTF8Tests, MalformedInputYieldsReplacementAndAdvancesOneByte)
    {
        const std::string cases[] = {
            "\x80",             // stray continuation byte
            "\xC3",             // truncated two-byte sequence
            "\xE2\x82",         // truncated three-byte sequence
            "\xC3\x28",         // bad continuation byte
            "\xC0\xAF",         // overlong encoding of '/'
            "\xE0\x80\xAF",     // overlong three-byte encoding
            "\xED\xA0\x80",     // UTF-16 surrogate
            "\xF4\x90\x80\x80", // above U+10FFFF
            "\xFF",             // invalid lead byte
        };
        for (const std::string& text : cases)
        {
            const UTF8Decoded decoded = DecodeUTF8(text, 0);
            EXPECT_EQ(decoded.Codepoint, ReplacementCharacter);
            EXPECT_EQ(decoded.Length, 1u);
        }
    }

    TEST(UTF8Tests, EncodeRoundTrips)
    {
        const char32_t codepoints[] = {0x24, 0x7F, 0x80, 0x7FF, 0x800, 0xFFFF, 0x10000, 0x10FFFF};
        for (char32_t codepoint : codepoints)
        {
            std::string text;
            AppendUTF8(text, codepoint);
            const UTF8Decoded decoded = DecodeUTF8(text, 0);
            EXPECT_EQ(decoded.Codepoint, codepoint);
            EXPECT_EQ(decoded.Length, text.size());
        }

        // Surrogates and out-of-range values are encoded as the replacement character.
        std::string invalid;
        AppendUTF8(invalid, 0xD800);
        EXPECT_EQ(DecodeUTF8(invalid, 0).Codepoint, ReplacementCharacter);
        EXPECT_EQ(invalid.size(), 3u);
    }

    TEST(UTF8Tests, StepsOverWholeCodepoints)
    {
        const std::string text = "a\xE2\x82\xAC\xF0\x9F\x98\x80z"; // a, €, 😀, z
        EXPECT_EQ(NextCodepointOffset(text, 0), 1u);
        EXPECT_EQ(NextCodepointOffset(text, 1), 4u);
        EXPECT_EQ(NextCodepointOffset(text, 4), 8u);
        EXPECT_EQ(NextCodepointOffset(text, 8), 9u);
        EXPECT_EQ(NextCodepointOffset(text, 9), 9u);

        EXPECT_EQ(PreviousCodepointOffset(text, 9), 8u);
        EXPECT_EQ(PreviousCodepointOffset(text, 8), 4u);
        EXPECT_EQ(PreviousCodepointOffset(text, 4), 1u);
        EXPECT_EQ(PreviousCodepointOffset(text, 1), 0u);
        EXPECT_EQ(PreviousCodepointOffset(text, 0), 0u);

        // Malformed trailing bytes are stepped over one at a time.
        const std::string broken = "a\x80\x80";
        EXPECT_EQ(PreviousCodepointOffset(broken, 3), 2u);
        EXPECT_EQ(PreviousCodepointOffset(broken, 2), 1u);
    }
} // namespace Carbon
