#include "Support/WidgetTest.h"

namespace Carbon
{
    // The three styling layers and their precedence: per-call option > style stack > theme.
    using StyleTests = WidgetTest;

    TEST_F(StyleTests, ThemeIsTheBaseLayer)
    {
        Frame(
            []
            {
                EXPECT_EQ(GetStyleColor(StyleColor::Accent), Theme::Light().GetColor(StyleColor::Accent));
                EXPECT_FLOAT_EQ(GetStyleVar(StyleVar::CornerRadius), 6.0f);
            });
    }

    TEST_F(StyleTests, StackOverridesTheThemeUntilPopped)
    {
        const Color orange = Color::FromHex(0xFF9F0A);
        Frame(
            [&]
            {
                PushStyleColor(StyleColor::Accent, orange);
                PushStyleVar(StyleVar::CornerRadius, 12.0f);
                EXPECT_EQ(GetStyleColor(StyleColor::Accent), orange);
                EXPECT_FLOAT_EQ(GetStyleVar(StyleVar::CornerRadius), 12.0f);
                // Other values are untouched, and the theme itself is not modified.
                EXPECT_EQ(GetStyleColor(StyleColor::Label), Theme::Light().GetColor(StyleColor::Label));
                EXPECT_EQ(GetTheme().GetColor(StyleColor::Accent), Theme::Light().GetColor(StyleColor::Accent));
                PopStyleVar();
                PopStyleColor();
                EXPECT_EQ(GetStyleColor(StyleColor::Accent), Theme::Light().GetColor(StyleColor::Accent));
                EXPECT_FLOAT_EQ(GetStyleVar(StyleVar::CornerRadius), 6.0f);
            });
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(StyleTests, NestedPushesRestoreInOrder)
    {
        const Color red = Color::FromHex(0xFF0000);
        const Color green = Color::FromHex(0x00FF00);
        Frame(
            [&]
            {
                PushStyleColor(StyleColor::Accent, red);
                PushStyleColor(StyleColor::Label, green);
                PushStyleColor(StyleColor::Accent, green); // the same color again, further in
                EXPECT_EQ(GetStyleColor(StyleColor::Accent), green);
                PopStyleColor();
                EXPECT_EQ(GetStyleColor(StyleColor::Accent), red);
                EXPECT_EQ(GetStyleColor(StyleColor::Label), green);
                PopStyleColor(2); // several at once
                EXPECT_EQ(GetStyleColor(StyleColor::Accent), Theme::Light().GetColor(StyleColor::Accent));
                EXPECT_EQ(GetStyleColor(StyleColor::Label), Theme::Light().GetColor(StyleColor::Label));
            });
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(StyleTests, PerCallValueBeatsTheStackWhichBeatsTheTheme)
    {
        const Color pushed = Color::FromHex(0x00FF00);
        const Color perCall = Color::FromHex(0xFF00FF);
        Frame(
            [&]
            {
                // Theme only.
                EXPECT_EQ(Resolve(std::optional<Color>(), StyleColor::Accent),
                          Theme::Light().GetColor(StyleColor::Accent));
                EXPECT_FLOAT_EQ(Resolve(std::optional<float>(), StyleVar::CornerRadius), 6.0f);

                PushStyleColor(StyleColor::Accent, pushed);
                PushStyleVar(StyleVar::CornerRadius, 12.0f);
                // Stack over theme.
                EXPECT_EQ(Resolve(std::optional<Color>(), StyleColor::Accent), pushed);
                EXPECT_FLOAT_EQ(Resolve(std::optional<float>(), StyleVar::CornerRadius), 12.0f);
                // Per-call over stack.
                EXPECT_EQ(Resolve(std::optional<Color>(perCall), StyleColor::Accent), perCall);
                EXPECT_FLOAT_EQ(Resolve(std::optional<float>(20.0f), StyleVar::CornerRadius), 20.0f);
                PopStyleVar();
                PopStyleColor();
            });
    }

    TEST_F(StyleTests, PrecedenceAppliesToWidgets)
    {
        // The corner radius of three buttons: from the theme, from the stack, and from the call.
        Settle(
            []
            {
                Button("Theme");
                PushStyleVar(StyleVar::CornerRadius, 10.0f);
                Button("Stack");
                Button("Call", {.CornerRadius = 3.0f});
                PopStyleVar();
            });
        // Each button draws its background squircle first: quads 0, 2 and 4 (label glyphs follow each).
        const DrawData& drawData = GetDrawData();
        std::vector<float> radii;
        for (const DrawPrimitive& primitive : drawData.Primitives)
        {
            if (primitive.Kind == DrawPrimitiveKind::Squircle)
                radii.push_back(primitive.Radius);
        }
        ASSERT_EQ(radii.size(), 3u);
        EXPECT_FLOAT_EQ(radii[0], 6.0f);
        EXPECT_FLOAT_EQ(radii[1], 10.0f);
        EXPECT_FLOAT_EQ(radii[2], 3.0f);
    }

    TEST_F(StyleTests, PushedColorsReachWidgetsAndStacks)
    {
        const Color tint = Color::FromHex(0xAF52DE);
        Rect first, second;
        Settle(
            [&]
            {
                PushStyleColor(StyleColor::Accent, tint);
                PushStyleVar(StyleVar::Spacing, 20.0f);
                BeginVStack();
                Button("Save", {.Role = ButtonRole::Prominent});
                first = GetItemRect();
                Button("Other");
                second = GetItemRect();
                EndVStack();
                PopStyleVar();
                PopStyleColor();
            });
        EXPECT_EQ(GetQuadColor(0), tint);
        EXPECT_FLOAT_EQ(second.Y - first.GetBottom(), 20.0f);
    }

    TEST_F(StyleTests, UnbalancedStacksAreReportedAtTheEndOfTheFrame)
    {
        Frame(
            []
            {
                PushStyleColor(StyleColor::Accent, Color::Black());
                PushStyleVar(StyleVar::Spacing, 1.0f);
                PushStyleVar(StyleVar::CornerRadius, 1.0f);
            });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Unbalanced style stack"), std::string::npos);
        EXPECT_NE(m_AssertMessages[0].find("1 PushStyleColor and 2 PushStyleVar"), std::string::npos);

        // The leak does not carry over into the next frame.
        m_AssertMessages.clear();
        Frame(
            []
            {
                EXPECT_EQ(GetStyleColor(StyleColor::Accent), Theme::Light().GetColor(StyleColor::Accent));
                EXPECT_FLOAT_EQ(GetStyleVar(StyleVar::Spacing), 8.0f);
            });
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(StyleTests, PopWithoutPushIsReported)
    {
        Frame([] { PopStyleColor(); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("PopStyleColor"), std::string::npos);

        m_AssertMessages.clear();
        Frame(
            []
            {
                PushStyleVar(StyleVar::Spacing, 1.0f);
                PopStyleVar(2); // one too many
            });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("PopStyleVar"), std::string::npos);
    }

    TEST_F(StyleTests, StackSitsOnTopOfARunningThemeTransition)
    {
        RunFrame();
        SetTheme(Theme::Dark());
        GetIO().SetDeltaTime(ThemeTransitionDuration * 0.5f);
        NewFrame();
        // The theme layer is mid-blend...
        EXPECT_NEAR(GetStyleColor(StyleColor::Background).R, 0.5f, 0.02f);
        // ...and a pushed value still wins over it.
        PushStyleColor(StyleColor::Background, Color::FromHex(0x123456));
        EXPECT_EQ(GetStyleColor(StyleColor::Background), Color::FromHex(0x123456));
        PopStyleColor();
        EXPECT_NEAR(GetStyleColor(StyleColor::Background).R, 0.5f, 0.02f);
        EndFrame();
    }

    TEST_F(StyleTests, BlendMixesTowardsAColor)
    {
        const Color base(0.2f, 0.4f, 0.6f, 0.5f);
        EXPECT_EQ(Blend(base, Color::White(), 0.0f), base);
        const Color half = Blend(base, Color::White(), 0.5f);
        EXPECT_FLOAT_EQ(half.R, 0.6f);
        EXPECT_FLOAT_EQ(half.A, 0.5f); // the base's alpha is kept
        EXPECT_FLOAT_EQ(Blend(base, Color::White(), 2.0f).R, 1.0f);
    }
} // namespace Carbon
