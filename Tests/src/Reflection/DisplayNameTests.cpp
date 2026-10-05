#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

#include "Carbon/Reflection/Detail/DisplayName.h"

namespace Carbon
{
    namespace
    {
        std::string Format(std::string_view identifier)
        {
            std::array<char, 128> buffer = {};
            const size_t length = Internal::FormatDisplayName(identifier, buffer);
            return std::string(buffer.data(), length);
        }
    } // namespace

    TEST(DisplayNameTests, SplitsPascalAndCamelCase)
    {
        EXPECT_EQ(Format("DarkMode"), "Dark Mode");
        EXPECT_EQ(Format("VeryHigh"), "Very High");
        EXPECT_EQ(Format("darkMode"), "Dark Mode");
        EXPECT_EQ(Format("TextureQuality"), "Texture Quality");
        EXPECT_EQ(Format("Gamma"), "Gamma");
        EXPECT_EQ(Format("x"), "X");
    }

    TEST(DisplayNameTests, KeepsAcronymsTogether)
    {
        EXPECT_EQ(Format("HDREnabled"), "HDR Enabled");
        EXPECT_EQ(Format("VSync"), "V Sync");
        EXPECT_EQ(Format("UserID"), "User ID");
        EXPECT_EQ(Format("IOError"), "IO Error");
        EXPECT_EQ(Format("HDR"), "HDR");
        EXPECT_EQ(Format("UseHTTPProxy"), "Use HTTP Proxy");
    }

    TEST(DisplayNameTests, DigitsStartAWordAfterLowercaseAndStayWithCapitals)
    {
        EXPECT_EQ(Format("Volume2"), "Volume 2");
        EXPECT_EQ(Format("Channel10Gain"), "Channel 10 Gain");
        EXPECT_EQ(Format("MP3Player"), "MP3 Player");
        EXPECT_EQ(Format("HTTP2Server"), "HTTP2 Server");
        EXPECT_EQ(Format("Vector3D"), "Vector 3D");
        EXPECT_EQ(Format("Option1A"), "Option 1A");
    }

    TEST(DisplayNameTests, UnderscoresSeparateWords)
    {
        EXPECT_EQ(Format("dark_mode"), "Dark Mode");
        EXPECT_EQ(Format("max_frame_rate"), "Max Frame Rate");
        EXPECT_EQ(Format("MAX_SIZE"), "MAX SIZE");
        EXPECT_EQ(Format("_private_"), "Private");
        EXPECT_EQ(Format("a__b"), "A B");
        EXPECT_EQ(Format("snake_Case2x"), "Snake Case 2x");
        EXPECT_EQ(Format(""), "");
        EXPECT_EQ(Format("___"), "");
    }

    TEST(DisplayNameTests, MinorWordsInsideALabelStayLowercase)
    {
        EXPECT_EQ(Format("LaunchAtLogin"), "Launch at Login");
        EXPECT_EQ(Format("DownloadInBackground"), "Download in Background");
        EXPECT_EQ(Format("show_in_finder"), "Show in Finder");
        EXPECT_EQ(Format("SaveAsCopy"), "Save as Copy");
        EXPECT_EQ(Format("RulesOfTheGame"), "Rules of the Game");
        // First and last words keep their capital; so do words in capitals and longer words.
        EXPECT_EQ(Format("OnStartup"), "On Startup");
        EXPECT_EQ(Format("SignIn"), "Sign In");
        EXPECT_EQ(Format("HDR_IN_Output"), "HDR IN Output");
        EXPECT_EQ(Format("PlayAboutSounds"), "Play About Sounds");
        EXPECT_EQ(Format("Infinity"), "Infinity");
    }

    TEST(DisplayNameTests, NeverWritesMoreThanTwiceTheIdentifier)
    {
        // The worst case is a word break before every character after the first.
        std::array<char, 6> buffer = {};
        EXPECT_EQ(Internal::FormatDisplayName("a_b_c", buffer), 5u);
        EXPECT_EQ(std::string_view(buffer.data(), 5), "A B C");
        std::array<char, 2> small = {};
        EXPECT_LE(Internal::FormatDisplayName("LongIdentifier", small), small.size());
    }

    TEST(DisplayNameTests, LabelTableUsesGivenNamesAndFormatsTheRest)
    {
        static constexpr std::array<std::string_view, 3> Identifiers = {"DarkMode", "VSync", "Gamma"};
        static constexpr std::array<std::string_view, 3> DisplayNames = {"", "Vertical Sync", ""};
        const Internal::LabelTable<3, Internal::GetLabelCapacity(Identifiers, DisplayNames)> table(Identifiers,
                                                                                                   DisplayNames);
        EXPECT_EQ(table.GetLabels()[0], "Dark Mode");
        EXPECT_EQ(table.GetLabels()[1], "Vertical Sync");
        EXPECT_EQ(table.GetLabels()[2], "Gamma");
        static_assert(Internal::GetLabelCapacity(Identifiers, DisplayNames) == 26);
    }
} // namespace Carbon
