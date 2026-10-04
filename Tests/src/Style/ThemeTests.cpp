#include "Support/ContextTest.h"

namespace Carbon
{
    using ThemeTests = ContextTest;

    TEST_F(ThemeTests, DarkThemeIsPureBlackWithWhiteText)
    {
        const Theme dark = Theme::Dark();
        EXPECT_TRUE(dark.IsDark);
        EXPECT_EQ(dark.GetColor(StyleColor::Background), Color::FromHex(0x000000));
        EXPECT_EQ(dark.GetColor(StyleColor::Label), Color::FromHex(0xFFFFFF));
        EXPECT_EQ(dark.GetColor(StyleColor::Accent), Color::FromHex(0x0A84FF));
    }

    TEST_F(ThemeTests, LightThemeIsWhiteWithBlackTextAndSystemBlue)
    {
        const Theme light = Theme::Light();
        EXPECT_FALSE(light.IsDark);
        EXPECT_EQ(light.GetColor(StyleColor::Background), Color::FromHex(0xFFFFFF));
        EXPECT_EQ(light.GetColor(StyleColor::Label), Color::FromHex(0x000000));
        EXPECT_EQ(light.GetColor(StyleColor::Accent), Color::FromHex(0x007AFF));
    }

    TEST_F(ThemeTests, EveryColorAndMetricIsDefined)
    {
        for (const Theme& theme : {Theme::Light(), Theme::Dark()})
        {
            for (size_t i = 0; i < theme.Colors.size(); i++)
                EXPECT_GT(theme.Colors[i].A, 0.0f) << "color " << i << " was left unset";
            for (size_t i = 0; i < theme.Vars.size(); i++)
                EXPECT_GT(theme.Vars[i], 0.0f) << "metric " << i << " was left unset";
            for (const TextStyleSpec& style : theme.TextStyles)
            {
                EXPECT_GE(style.Size, 10.0f); // the HIG's minimum text size on macOS
                EXPECT_GT(style.LineHeight, style.Size);
            }
        }
        EXPECT_FLOAT_EQ(Theme::Light().GetVar(StyleVar::CornerSmoothing), 0.6f);
    }

    TEST_F(ThemeTests, TextMeetsTheContrastMinimum)
    {
        // Relative luminance and contrast ratio as defined by WCAG; the HIG asks for at least 4.5:1.
        const auto luminance = [](const Color& color)
        {
            const auto linear = [](float channel)
            { return channel <= 0.04045f ? channel / 12.92f : std::pow((channel + 0.055f) / 1.055f, 2.4f); };
            return 0.2126f * linear(color.R) + 0.7152f * linear(color.G) + 0.0722f * linear(color.B);
        };
        const auto contrast = [&](const Color& a, const Color& b)
        {
            const float brighter = std::max(luminance(a), luminance(b));
            const float darker = std::min(luminance(a), luminance(b));
            return (brighter + 0.05f) / (darker + 0.05f);
        };

        // What a translucent color looks like on top of an opaque one.
        const auto over = [](const Color& top, const Color& bottom)
        {
            return Color(bottom.R + (top.R - bottom.R) * top.A, bottom.G + (top.G - bottom.G) * top.A,
                         bottom.B + (top.B - bottom.B) * top.A);
        };

        for (const Theme& theme : {Theme::Light(), Theme::Dark()})
        {
            for (const StyleColor surface : {StyleColor::Background, StyleColor::SecondaryBackground})
            {
                const Color background = theme.GetColor(surface);
                EXPECT_GE(contrast(theme.GetColor(StyleColor::Label), background), 4.5f);
                EXPECT_GE(contrast(theme.GetColor(StyleColor::SecondaryLabel), background), 4.5f);

                // Labels on controls (a button's fill on that surface) stay readable too.
                const Color control = over(theme.GetColor(StyleColor::ControlFill), background);
                EXPECT_GE(contrast(theme.GetColor(StyleColor::Label), control), 4.5f);
                EXPECT_GE(contrast(theme.GetColor(StyleColor::SecondaryLabel), control), 3.0f);
            }
        }
    }

    TEST_F(ThemeTests, FirstThemeAppliesImmediately)
    {
        // No frame has run yet: there is nothing to animate from.
        SetTheme(Theme::Dark());
        EXPECT_EQ(GetStyleColor(StyleColor::Background), Color::FromHex(0x000000));
        RunFrame();
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(ThemeTests, SwitchingAnimatesEveryColorAndMetric)
    {
        Theme rounder = Theme::Dark();
        rounder.SetVar(StyleVar::CornerRadius, 16.0f);
        RunFrame();
        SetTheme(rounder);

        // Still the old values until the next frame advances the transition.
        EXPECT_EQ(GetStyleColor(StyleColor::Background), Color::FromHex(0xFFFFFF));
        EXPECT_EQ(GetTargetTheme().GetColor(StyleColor::Background), Color::FromHex(0x000000));

        // Half-way through, colors and metrics are between the two themes.
        RunFrame(ThemeTransitionDuration * 0.5f);
        EXPECT_TRUE(IsAnimating());
        const Color background = GetStyleColor(StyleColor::Background);
        EXPECT_NEAR(background.R, 0.5f, 0.02f);
        EXPECT_NEAR(background.G, 0.5f, 0.02f);
        EXPECT_NEAR(GetStyleColor(StyleColor::Label).R, 0.5f, 0.02f);
        EXPECT_NEAR(GetStyleVar(StyleVar::CornerRadius), 11.0f, 0.2f);
        // The appearance flag belongs to where the transition is heading.
        EXPECT_TRUE(GetTheme().IsDark);

        // The transition is monotonic and ends exactly on the target.
        float previous = background.R;
        for (int i = 0; i < 30; i++)
        {
            RunFrame();
            const float current = GetStyleColor(StyleColor::Background).R;
            EXPECT_LE(current, previous + 1e-5f);
            previous = current;
        }
        EXPECT_EQ(GetStyleColor(StyleColor::Background), Color::FromHex(0x000000));
        EXPECT_FLOAT_EQ(GetStyleVar(StyleVar::CornerRadius), 16.0f);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(ThemeTests, InterruptedSwitchStartsFromTheCurrentBlend)
    {
        RunFrame();
        SetTheme(Theme::Dark());
        RunFrame(ThemeTransitionDuration * 0.5f);
        const Color midway = GetStyleColor(StyleColor::Background);

        // Back to light in the middle of the transition: no jump.
        SetTheme(Theme::Light());
        EXPECT_EQ(GetStyleColor(StyleColor::Background), midway);
        RunFrame(0.01f);
        const Color after = GetStyleColor(StyleColor::Background);
        EXPECT_NEAR(after.R, midway.R, 0.05f);
        EXPECT_GE(after.R, midway.R);

        for (int i = 0; i < 40; i++)
            RunFrame();
        EXPECT_EQ(GetStyleColor(StyleColor::Background), Color::FromHex(0xFFFFFF));
    }

    TEST_F(ThemeTests, SwitchWithoutAnimationIsInstant)
    {
        RunFrame();
        SetTheme(Theme::Dark(), false);
        EXPECT_EQ(GetStyleColor(StyleColor::Background), Color::FromHex(0x000000));
        RunFrame();
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(ThemeTests, ReduceMotionShortensTheSwitchToACrossFade)
    {
        SetReduceMotion(true);
        RunFrame();
        SetTheme(Theme::Dark());
        RunFrame(ReducedMotionFadeDuration * 0.5f);
        EXPECT_NEAR(GetStyleColor(StyleColor::Background).R, 0.5f, 0.02f);
        RunFrame(ReducedMotionFadeDuration);
        EXPECT_EQ(GetStyleColor(StyleColor::Background), Color::FromHex(0x000000));
    }

    TEST_F(ThemeTests, CustomThemesChangeColorsAndTheTypeRamp)
    {
        Theme theme = Theme::Light();
        theme.SetColor(StyleColor::Accent, Color::FromHex(0xFF9F0A));
        theme.SetTextStyle(TextStyle::Body, {15.0f, 20.0f, FontWeight::Medium, FontWeight::Bold});
        SetTheme(theme);

        EXPECT_EQ(GetStyleColor(StyleColor::Accent), Color::FromHex(0xFF9F0A));
        const TextSpec body = GetTextSpec(TextStyle::Body);
        EXPECT_FLOAT_EQ(body.Size, 15.0f);
        EXPECT_FLOAT_EQ(body.LineHeight, 20.0f);
        EXPECT_EQ(body.Weight, FontWeight::Medium);
        EXPECT_EQ(GetTextSpec(TextStyle::Body, true).Weight, FontWeight::Bold);
        // Untouched styles keep the HIG values.
        EXPECT_FLOAT_EQ(GetTextSpec(TextStyle::LargeTitle).Size, 26.0f);
    }

    TEST_F(ThemeTests, LerpInterpolatesColorsAndClampsAtTheEnds)
    {
        const Theme light = Theme::Light();
        const Theme dark = Theme::Dark();
        EXPECT_EQ(Lerp(light, dark, 0.0f).GetColor(StyleColor::Background), light.GetColor(StyleColor::Background));
        EXPECT_EQ(Lerp(light, dark, 1.0f).GetColor(StyleColor::Background), dark.GetColor(StyleColor::Background));
        EXPECT_EQ(Lerp(light, dark, -1.0f).GetColor(StyleColor::Label), light.GetColor(StyleColor::Label));
        EXPECT_NEAR(Lerp(light, dark, 0.25f).GetColor(StyleColor::Background).R, 0.75f, 1e-4f);
    }

    using StateTests = ContextTest;

    TEST_F(StateTests, StateIsZeroInitializedAndPersistsWhileUsed)
    {
        struct Counter
        {
            int Count;
            float Value;
        };
        const ID id = HashID("counter");

        bool created = false;
        Frame(
            [&]
            {
                Counter* counter = GetState<Counter>(id, StateLifetime::Transient, &created);
                EXPECT_TRUE(created);
                EXPECT_EQ(counter->Count, 0);
                EXPECT_FLOAT_EQ(counter->Value, 0.0f);
                counter->Count = 5;
            });
        Frame(
            [&]
            {
                Counter* counter = GetState<Counter>(id, StateLifetime::Transient, &created);
                EXPECT_FALSE(created);
                EXPECT_EQ(counter->Count, 5);
            });
    }

    TEST_F(StateTests, TransientStateIsDroppedAfterAnUnusedFrame)
    {
        const ID id = HashID("transient");
        Frame([&] { *GetState<int>(id) = 7; });
        RunFrame(); // not requested during this frame
        bool created = false;
        Frame(
            [&]
            {
                EXPECT_EQ(*GetState<int>(id, StateLifetime::Transient, &created), 0);
                EXPECT_TRUE(created);
            });
    }

    TEST_F(StateTests, PersistentStateSurvivesUnusedFrames)
    {
        const ID id = HashID("persistent");
        Frame([&] { *GetState<int>(id, StateLifetime::Persistent) = 7; });
        for (int i = 0; i < 10; i++)
            RunFrame();
        Frame([&] { EXPECT_EQ(*GetState<int>(id, StateLifetime::Persistent), 7); });
    }

    TEST_F(StateTests, DifferentTypesAndIDsDoNotShareStorage)
    {
        struct First
        {
            int Value;
        };
        struct Second
        {
            int Value;
        };
        const ID a = HashID("a");
        const ID b = HashID("b");
        Frame(
            [&]
            {
                GetState<First>(a)->Value = 1;
                GetState<Second>(a)->Value = 2;
                GetState<First>(b)->Value = 3;
                EXPECT_EQ(GetState<First>(a)->Value, 1);
                EXPECT_EQ(GetState<Second>(a)->Value, 2);
                EXPECT_EQ(GetState<First>(b)->Value, 3);
            });
    }
} // namespace Carbon
