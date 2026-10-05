#include "Support/WidgetTest.h"

#include <array>
#include <string>
#include <vector>

#include "Carbon/Reflection/Reflection.h"

namespace ReflectTestTypes
{
    enum class Quality
    {
        Low,
        Medium,
        High
    };

    struct Settings
    {
        bool VSync = true;
        Quality TextureQuality = Quality::Low;
        float Gamma = 2.0f;
        int Samples = 4;
    };
    CB_REFLECT_STRUCT(Settings, CB_FIELD(Gamma, {.Min = 1.0, .Max = 3.0, .Step = 0.5}),
                      CB_FIELD(Samples, {.Min = 1, .Max = 16, .Control = Carbon::ReflectControl::Stepper}));

    struct Locked
    {
        bool First = false;
        bool Second = false;
        bool Third = false;
    };
    CB_REFLECT_STRUCT(Locked, CB_FIELD(First, {.ReadOnly = true}), CB_FIELD(Second, {.Hidden = true}));

    struct OnlyThird
    {
        bool First = false;
        bool Third = false;
    };
    CB_REFLECT_STRUCT(OnlyThird, CB_FIELD(First, {.ReadOnly = true}));

    struct Inner
    {
        bool Enabled = false;
        double Level = 0.5;
    };

    struct Outer
    {
        std::string Name = "Carbon";
        Inner Details = {};
        bool Last = false;
    };

    struct Annotated
    {
        bool Muted = false;
    };
    CB_REFLECT_STRUCT(Annotated, CB_FIELD(Muted, {.Tooltip = "Silences every sound"}));
} // namespace ReflectTestTypes

namespace Carbon
{
    using namespace ReflectTestTypes;

    class ReflectTests : public WidgetTest
    {
    protected:
        // Runs a frame of the given interface and records what Reflect returned.
        template <typename T>
        void Show(T* value, const ReflectOptions& options = {})
        {
            Frame(
                [&]
                {
                    BeginVStack({.Padding = 20.0f});
                    m_Results.push_back(Reflect("value", value, options));
                    EndVStack();
                });
        }

        // Shows the interface until its layout has settled.
        template <typename T>
        void SettleOn(T* value, const ReflectOptions& options = {})
        {
            for (int i = 0; i < 20; i++)
                Show(value, options);
        }

        template <typename T>
        void TapKey(Key key, T* value, const ReflectOptions& options = {})
        {
            GetIO().AddKeyEvent(key, true);
            Show(value, options);
            GetIO().AddKeyEvent(key, false);
            Show(value, options);
        }

        int CountChanges() const
        {
            int count = 0;
            for (const bool result : m_Results)
                count += result ? 1 : 0;
            return count;
        }

        std::vector<bool> m_Results;
    };

    TEST_F(ReflectTests, ReturnsTrueExactlyOnTheFramesAFieldChanges)
    {
        Settings settings;
        SettleOn(&settings);
        EXPECT_EQ(CountChanges(), 0);

        // Tab to the switch and flip it with Space.
        TapKey(Key::Tab, &settings);
        TapKey(Key::Space, &settings);
        EXPECT_FALSE(settings.VSync);
        EXPECT_EQ(CountChanges(), 1);

        // The pop-up button: Space opens its menu, Down and Return pick the next value.
        m_Results.clear();
        TapKey(Key::Tab, &settings);
        TapKey(Key::Space, &settings);
        TapKey(Key::DownArrow, &settings);
        TapKey(Key::Enter, &settings);
        EXPECT_EQ(settings.TextureQuality, Quality::Medium);
        EXPECT_EQ(CountChanges(), 1);

        // The slider moves by its step.
        m_Results.clear();
        TapKey(Key::Tab, &settings);
        TapKey(Key::RightArrow, &settings);
        EXPECT_FLOAT_EQ(settings.Gamma, 2.5f);
        EXPECT_EQ(CountChanges(), 1);

        // The stepper: up adds one.
        m_Results.clear();
        TapKey(Key::Tab, &settings);
        TapKey(Key::UpArrow, &settings);
        EXPECT_EQ(settings.Samples, 5);
        EXPECT_EQ(CountChanges(), 1);

        // Nothing else happens without input.
        m_Results.clear();
        for (int i = 0; i < 10; i++)
            Show(&settings);
        EXPECT_EQ(CountChanges(), 0);
        EXPECT_TRUE(m_AssertMessages.empty()) << (m_AssertMessages.empty() ? std::string() : m_AssertMessages.front());
    }

    TEST_F(ReflectTests, EnumStylesEditAnEnumOnItsOwn)
    {
        for (const ReflectEnumStyle style :
             {ReflectEnumStyle::PopUpButton, ReflectEnumStyle::SegmentedControl, ReflectEnumStyle::RadioGroup})
        {
            Quality quality = Quality::Low;
            m_Results.clear();
            const ReflectOptions options = {.EnumStyle = style};
            SettleOn(&quality, options);
            m_Results.clear();
            TapKey(Key::Tab, &quality, options);
            if (style == ReflectEnumStyle::PopUpButton)
            {
                // The menu opens with Space; Down and Return pick the next item.
                TapKey(Key::Space, &quality, options);
                TapKey(Key::DownArrow, &quality, options);
                TapKey(Key::Enter, &quality, options);
            }
            else
            {
                TapKey(style == ReflectEnumStyle::RadioGroup ? Key::DownArrow : Key::RightArrow, &quality, options);
            }
            EXPECT_EQ(quality, Quality::Medium) << static_cast<int>(style);
            EXPECT_EQ(CountChanges(), 1) << static_cast<int>(style);
            TapKey(Key::Tab, &quality, options); // leave the control for the next style
        }
        EXPECT_TRUE(m_AssertMessages.empty()) << (m_AssertMessages.empty() ? std::string() : m_AssertMessages.front());
    }

    TEST_F(ReflectTests, ReadOnlyFieldsAreSkippedByTabAndHiddenFieldsAreNotDrawn)
    {
        Locked locked;
        SettleOn(&locked);
        // The read-only switch is disabled and the hidden one absent: the first Tab reaches Third.
        TapKey(Key::Tab, &locked);
        TapKey(Key::Space, &locked);
        EXPECT_FALSE(locked.First);
        EXPECT_FALSE(locked.Second);
        EXPECT_TRUE(locked.Third);

        // A hidden field draws nothing: the same as a struct without it.
        SettleOn(&locked);
        const size_t withHidden = GetDrawData().Vertices.size();
        OnlyThird only = {.First = locked.First, .Third = locked.Third};
        SettleOn(&only);
        EXPECT_EQ(GetDrawData().Vertices.size(), withHidden);
    }

    TEST_F(ReflectTests, NestedStructsAreEditableAndReportChanges)
    {
        Outer outer;
        SettleOn(&outer);
        m_Results.clear();
        // Name (text field), then the nested Enabled switch.
        TapKey(Key::Tab, &outer);
        TapKey(Key::Tab, &outer);
        TapKey(Key::Space, &outer);
        EXPECT_TRUE(outer.Details.Enabled);
        EXPECT_EQ(CountChanges(), 1);
        EXPECT_TRUE(m_AssertMessages.empty()) << (m_AssertMessages.empty() ? std::string() : m_AssertMessages.front());
    }

    TEST_F(ReflectTests, BothLayoutsDrawEveryFieldAndDisabledBlocksInput)
    {
        Settings settings;
        SettleOn(&settings, {.Layout = ReflectLayout::LabelAbove});
        TapKey(Key::Tab, &settings, {.Layout = ReflectLayout::LabelAbove});
        TapKey(Key::Space, &settings, {.Layout = ReflectLayout::LabelAbove});
        EXPECT_FALSE(settings.VSync);

        // Disabled: nothing takes focus or changes.
        m_Results.clear();
        const ReflectOptions disabled = {.Disabled = true};
        SettleOn(&settings, disabled);
        TapKey(Key::Tab, &settings, disabled);
        TapKey(Key::Space, &settings, disabled);
        EXPECT_FALSE(settings.VSync);
        EXPECT_EQ(CountChanges(), 0);
        EXPECT_TRUE(m_AssertMessages.empty()) << (m_AssertMessages.empty() ? std::string() : m_AssertMessages.front());
    }

    TEST_F(ReflectTests, LabelAboveIsTallerAndLabelLeadingWider)
    {
        Settings settings;
        Rect leading;
        Rect above;
        Settle(
            [&]
            {
                BeginVStack();
                Reflect("value", &settings);
                EndVStack();
                leading = GetLastItemRect();
            });
        Settle(
            [&]
            {
                BeginVStack();
                Reflect("value", &settings, {.Layout = ReflectLayout::LabelAbove});
                EndVStack();
                above = GetLastItemRect();
            });
        EXPECT_GT(above.Height, leading.Height);
        EXPECT_GT(leading.Width, 0.0f);
    }

    TEST(ReflectNumberTests, NumbersShowAtMostTwoDecimals)
    {
        std::array<char, 48> buffer = {};
        const auto format = [&](double value, bool isInteger)
        { return std::string(Internal::FormatReflectedNumber(value, isInteger, buffer)); };
        EXPECT_EQ(format(2.2, false), "2.2");
        EXPECT_EQ(format(0.75, false), "0.75");
        EXPECT_EQ(format(1.0, false), "1");
        EXPECT_EQ(format(3.14159, false), "3.14");
        EXPECT_EQ(format(2.999, false), "3");
        EXPECT_EQ(format(-0.001, false), "0");
        EXPECT_EQ(format(-12.5, false), "-12.5");
        EXPECT_EQ(format(1000000.0, false), "1000000");
        EXPECT_EQ(format(42.0, true), "42");
        EXPECT_EQ(format(-7.0, true), "-7");
        EXPECT_EQ(format(41.6, true), "42");
        std::array<char, 3> small = {};
        EXPECT_EQ(Internal::FormatReflectedNumber(12345.0, true, small).size(), 3u);
    }

    TEST_F(ReflectTests, ATooltipAppearsOverTheRow)
    {
        Annotated annotated;
        const auto build = [&]
        {
            BeginVStack({.Padding = 20.0f});
            Reflect("annotated", &annotated);
            EndVStack();
        };
        Settle(build);
        const size_t quiet = GetDrawData().Vertices.size();
        // Rest the pointer on the label, left of the switch, past the tooltip delay.
        GetIO().AddMousePosEvent(30.0f, 30.0f);
        Settle(build, 60);
        EXPECT_GT(GetDrawData().Vertices.size(), quiet);
    }
} // namespace Carbon
