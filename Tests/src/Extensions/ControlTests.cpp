#include <Carbon/Extensions/Extensions.h>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // ---- SegmentedControl ---------------------------------------------------------------------------------------

    class SegmentedControlTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                if (SegmentedControl("segments", &m_Selected, {"One", "Two", "Three"}, m_Options))
                    m_Changes++;
                m_Rect = GetItemRect();
                m_Id = GetItemID();
            };
        }

        Vec2 GetSegmentCenter(int index) const
        {
            return Vec2(m_Rect.X + m_Rect.Width * (float(index) + 0.5f) / 3.0f, m_Rect.GetCenter().Y);
        }

        SegmentedControlOptions m_Options;
        int m_Selected = 0;
        int m_Changes = 0;
        Rect m_Rect;
        ID m_Id;
    };

    TEST_F(SegmentedControlTests, ClickSelectsASegment)
    {
        Settle(Interface());
        Click(GetSegmentCenter(2), Interface());
        EXPECT_EQ(m_Selected, 2);
        EXPECT_EQ(m_Changes, 1);

        // Clicking the selected segment again changes nothing.
        Click(GetSegmentCenter(2), Interface());
        EXPECT_EQ(m_Changes, 1);
    }

    TEST_F(SegmentedControlTests, ArrowKeysMoveTheSelectionWhileFocused)
    {
        Settle(Interface());
        TapKey(Key::RightArrow, Interface());
        EXPECT_EQ(m_Selected, 0) << "without focus the keys do nothing";

        TapKey(Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), m_Id);
        TapKey(Key::RightArrow, Interface());
        TapKey(Key::RightArrow, Interface());
        EXPECT_EQ(m_Selected, 2);
        TapKey(Key::RightArrow, Interface());
        EXPECT_EQ(m_Selected, 2) << "the selection stops at the last segment";
        EXPECT_EQ(m_Changes, 2);
        TapKey(Key::LeftArrow, Interface());
        EXPECT_EQ(m_Selected, 1);
    }

    TEST_F(SegmentedControlTests, IsOneTabStopAndClickingFocusesIt)
    {
        Settle(Interface());
        Click(GetSegmentCenter(1), Interface());
        EXPECT_EQ(GetFocusedID(), m_Id);
        EXPECT_FALSE(IsFocusVisible(m_Id));
    }

    TEST_F(SegmentedControlTests, FitsItsWidestLabelAndFillsOnRequest)
    {
        Settle(Interface());
        const float fitWidth = m_Rect.Width;
        EXPECT_GT(fitWidth, 0.0f);
        EXPECT_LT(fitWidth, 400.0f);

        m_Options.Width = Size::Fill();
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Rect.Width, 800.0f);
    }

    TEST_F(SegmentedControlTests, OutOfRangeSelectionIsClamped)
    {
        m_Selected = 7;
        Settle(Interface());
        EXPECT_TRUE(m_AssertMessages.empty());
        Click(GetSegmentCenter(0), Interface());
        EXPECT_EQ(m_Selected, 0);
    }

    TEST_F(SegmentedControlTests, DisabledIgnoresInput)
    {
        m_Options.Disabled = true;
        Settle(Interface());
        Click(GetSegmentCenter(2), Interface());
        EXPECT_EQ(m_Selected, 0);
        TapKey(Key::Tab, Interface());
        EXPECT_FALSE(GetFocusedID().IsValid());
    }

    TEST_F(SegmentedControlTests, SelectionSlidesAndSettles)
    {
        Settle(Interface());
        Click(GetSegmentCenter(2), Interface());
        EXPECT_TRUE(IsAnimating());
        Settle(Interface(), 120);
        EXPECT_FALSE(IsAnimating());
    }

    // ---- RadioGroup ---------------------------------------------------------------------------------------------

    class RadioGroupTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                if (RadioGroup("radios", &m_Selected, {"One", "Two", "Three"}, m_Options))
                    m_Changes++;
                m_Rect = GetItemRect();
                m_Id = GetItemID();
            };
        }

        // The centre of a button's label: the whole row of a button is its hit target.
        Vec2 GetItemCenter(int index) const
        {
            if (m_Options.Orientation == Axis::Horizontal)
            {
                const float width = (m_Rect.Width - 40.0f) / 3.0f;
                return Vec2(m_Rect.X + (width + 20.0f) * float(index) + width * 0.5f, m_Rect.GetCenter().Y);
            }
            const float height = (m_Rect.Height - 12.0f) / 3.0f;
            return Vec2(m_Rect.X + 30.0f, m_Rect.Y + (height + 6.0f) * float(index) + height * 0.5f);
        }

        RadioGroupOptions m_Options;
        int m_Selected = 0;
        int m_Changes = 0;
        Rect m_Rect;
        ID m_Id;
    };

    TEST_F(RadioGroupTests, ClickSelectsAButton)
    {
        Settle(Interface());
        Click(GetItemCenter(2), Interface());
        EXPECT_EQ(m_Selected, 2);
        EXPECT_EQ(m_Changes, 1);
        Click(GetItemCenter(2), Interface());
        EXPECT_EQ(m_Changes, 1) << "clicking the selected button changes nothing";
        Click(GetItemCenter(0), Interface());
        EXPECT_EQ(m_Selected, 0);
        EXPECT_EQ(m_Changes, 2);
    }

    TEST_F(RadioGroupTests, IsOneTabStopAndArrowsMoveTheSelection)
    {
        Settle(Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 0) << "without focus the keys do nothing";

        TapKey(Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), m_Id);
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 1);
        TapKey(Key::RightArrow, Interface());
        EXPECT_EQ(m_Selected, 2);
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 2) << "the selection stops at the last button";
        TapKey(Key::UpArrow, Interface());
        TapKey(Key::LeftArrow, Interface());
        EXPECT_EQ(m_Selected, 0);
        EXPECT_EQ(m_Changes, 4);
    }

    TEST_F(RadioGroupTests, ClickingFocusesTheGroupWithoutARing)
    {
        Settle(Interface());
        Click(GetItemCenter(1), Interface());
        EXPECT_EQ(GetFocusedID(), m_Id);
        EXPECT_FALSE(IsFocusVisible(m_Id));
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 2);
    }

    TEST_F(RadioGroupTests, OutOfRangeIsClampedAndMinusOneSelectsNothing)
    {
        m_Selected = 7;
        Settle(Interface());
        EXPECT_EQ(m_Selected, 2);
        EXPECT_EQ(m_Changes, 0) << "clamping is not a change by the user";
        EXPECT_TRUE(m_AssertMessages.empty());

        m_Selected = -1;
        Settle(Interface());
        EXPECT_EQ(m_Selected, -1);
        TapKey(Key::Tab, Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 0) << "the first arrow selects the first button";
        EXPECT_EQ(m_Changes, 1);
    }

    TEST_F(RadioGroupTests, HorizontalButtonsAreEquallyWide)
    {
        m_Options.Orientation = Axis::Horizontal;
        Settle(Interface());
        EXPECT_GT(m_Rect.Width, m_Rect.Height * 6.0f);
        EXPECT_LT(m_Rect.Height, 20.0f);
        Click(GetItemCenter(1), Interface());
        EXPECT_EQ(m_Selected, 1);
        Click(GetItemCenter(2), Interface());
        EXPECT_EQ(m_Selected, 2);
    }

    TEST_F(RadioGroupTests, DisabledIgnoresInput)
    {
        m_Options.Disabled = true;
        Settle(Interface());
        Click(GetItemCenter(2), Interface());
        EXPECT_EQ(m_Selected, 0);
        TapKey(Key::Tab, Interface());
        EXPECT_FALSE(GetFocusedID().IsValid());
    }

    // ---- Stepper ------------------------------------------------------------------------------------------------

    class StepperTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                if (Stepper("stepper", &m_Value, m_Options))
                    m_Changes++;
                m_Rect = GetItemRect();
            };
        }

        Vec2 GetUp() const { return Vec2(m_Rect.GetCenter().X, m_Rect.Y + m_Rect.Height * 0.25f); }
        Vec2 GetDown() const { return Vec2(m_Rect.GetCenter().X, m_Rect.Y + m_Rect.Height * 0.75f); }

        StepperOptions m_Options;
        double m_Value = 5.0;
        int m_Changes = 0;
        Rect m_Rect;
    };

    TEST_F(StepperTests, HalvesStepUpAndDown)
    {
        Settle(Interface());
        Click(GetUp(), Interface());
        EXPECT_DOUBLE_EQ(m_Value, 6.0);
        Click(GetDown(), Interface());
        Click(GetDown(), Interface());
        EXPECT_DOUBLE_EQ(m_Value, 4.0);
        EXPECT_EQ(m_Changes, 3);
    }

    TEST_F(StepperTests, StaysInRange)
    {
        m_Options.Max = 6.0;
        Settle(Interface());
        Click(GetUp(), Interface());
        Click(GetUp(), Interface());
        EXPECT_DOUBLE_EQ(m_Value, 6.0);
        EXPECT_EQ(m_Changes, 1);
    }

    TEST_F(StepperTests, WrapsAroundOnRequest)
    {
        m_Options.Min = 1.0;
        m_Options.Max = 5.0;
        m_Options.Wraps = true;
        Settle(Interface());
        Click(GetUp(), Interface());
        EXPECT_DOUBLE_EQ(m_Value, 1.0);
        Click(GetDown(), Interface());
        EXPECT_DOUBLE_EQ(m_Value, 5.0);
    }

    TEST_F(StepperTests, UsesTheStep)
    {
        m_Options.Step = 0.25;
        Settle(Interface());
        Click(GetUp(), Interface());
        EXPECT_DOUBLE_EQ(m_Value, 5.25);
    }

    TEST_F(StepperTests, HoldingAButtonRepeats)
    {
        Settle(Interface());
        MoveMouse(GetUp(), Interface());
        PressMouse(Interface());
        EXPECT_DOUBLE_EQ(m_Value, 6.0);
        for (int i = 0; i < 10; i++)
            Frame(Interface(), 0.1f);
        ReleaseMouse(Interface());
        EXPECT_GT(m_Value, 8.0);
        const double afterRelease = m_Value;
        Settle(Interface());
        EXPECT_DOUBLE_EQ(m_Value, afterRelease);
    }

    TEST_F(StepperTests, ArrowKeysStepWhileFocused)
    {
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        TapKey(Key::UpArrow, Interface());
        TapKey(Key::UpArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_DOUBLE_EQ(m_Value, 6.0);
    }

    TEST_F(StepperTests, WorksWithIntegers)
    {
        int count = 3;
        Rect rect;
        const Builder build = [&]
        {
            Stepper("count", &count, {.Min = 0.0, .Max = 4.0});
            rect = GetItemRect();
        };
        Settle(build);
        const Vec2 up(rect.GetCenter().X, rect.Y + rect.Height * 0.25f);
        Click(up, build);
        Click(up, build);
        EXPECT_EQ(count, 4);
    }

    // ---- ProgressIndicator --------------------------------------------------------------------------------------

    class ProgressIndicatorTests : public WidgetTest
    {
    };

    TEST_F(ProgressIndicatorTests, BarFillsInProportion)
    {
        Rect rect;
        const Builder build = [&]
        {
            ProgressIndicator(0.25f, {.Width = 200.0f});
            rect = GetItemRect();
        };
        Settle(build);
        EXPECT_FLOAT_EQ(rect.Width, 200.0f);
        EXPECT_FALSE(IsAnimating());

        // Two quads: the track and the fill, each one point larger than its shape on every side.
        const DrawData& drawData = GetDrawData();
        ASSERT_EQ(drawData.Vertices.size(), 8u);
        EXPECT_FLOAT_EQ(drawData.Vertices[4].Position.X, rect.X - 1.0f);
        EXPECT_FLOAT_EQ(drawData.Vertices[6].Position.X, rect.X + 50.0f + 1.0f);
    }

    TEST_F(ProgressIndicatorTests, ValueIsClamped)
    {
        const Builder build = []
        {
            ProgressIndicator(-1.0f, {.Width = 200.0f});
            ProgressIndicator(7.0f, {.Width = 200.0f});
        };
        Settle(build);
        // An empty bar is only its track; a full one has a fill as long as the track.
        const DrawData& drawData = GetDrawData();
        ASSERT_EQ(drawData.Vertices.size(), 12u);
        EXPECT_FLOAT_EQ(drawData.Vertices[10].Position.X - drawData.Vertices[8].Position.X, 202.0f);
    }

    TEST_F(ProgressIndicatorTests, IndeterminateKeepsAnimating)
    {
        const Builder bar = [] { ProgressIndicator(0.0f, {.IsIndeterminate = true}); };
        Settle(bar, 60);
        EXPECT_TRUE(IsAnimating());

        const Builder spinner = []
        { ProgressIndicator(0.0f, {.Kind = ProgressKind::Spinner, .IsIndeterminate = true}); };
        Settle(spinner, 60);
        EXPECT_TRUE(IsAnimating());
        // Eight spokes.
        EXPECT_EQ(GetDrawData().Vertices.size(), 32u);
    }

    TEST_F(ProgressIndicatorTests, SpinnerHasAFixedSize)
    {
        Rect rect;
        const Builder build = [&]
        {
            ProgressIndicator(0.5f, {.Kind = ProgressKind::Spinner, .Width = Size::Fill()});
            rect = GetItemRect();
        };
        Settle(build);
        EXPECT_EQ(rect.GetSize(), Vec2(16.0f, 16.0f));
        EXPECT_FALSE(IsAnimating());
    }

    // ---- SearchField --------------------------------------------------------------------------------------------

    class SearchFieldTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                if (SearchField("search", &m_Text))
                    m_Changes++;
                m_Rect = GetItemRect();
            };
        }

        std::string m_Text;
        int m_Changes = 0;
        Rect m_Rect;
    };

    TEST_F(SearchFieldTests, TypingChangesTheText)
    {
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        Type("carbon", Interface());
        EXPECT_EQ(m_Text, "carbon");
        EXPECT_EQ(m_Changes, 1);
    }

    TEST_F(SearchFieldTests, EscapeClearsTheText)
    {
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        Type("carbon", Interface());
        TapKey(Key::Escape, Interface());
        EXPECT_TRUE(m_Text.empty());
        EXPECT_EQ(m_Changes, 2);

        // On an empty field Escape reports no change.
        Click(m_Rect.GetCenter(), Interface());
        TapKey(Key::Escape, Interface());
        EXPECT_EQ(m_Changes, 2);
    }

    // ---- ColorWell ----------------------------------------------------------------------------------------------

    class ColorWellTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                // Away from the display's edges, so that the popover sits centered below the well.
                BeginVStack({.Padding = EdgeInsets(300.0f, 100.0f)});
                if (ColorWell("color", &m_Color, m_Options))
                    m_Changes++;
                m_Rect = GetItemRect();
                EndVStack();
            };
        }

        ColorWellOptions m_Options;
        Color m_Color = Color::FromHex(0x123456);
        int m_Changes = 0;
        Rect m_Rect;
    };

    TEST_F(ColorWellTests, ClickOpensThePopover)
    {
        Settle(Interface());
        EXPECT_FALSE(IsAnyOverlayOpen());
        Click(m_Rect.GetCenter(), Interface());
        EXPECT_TRUE(IsAnyOverlayOpen());
        EXPECT_EQ(m_Changes, 0);

        TapKey(Key::Escape, Interface());
        EXPECT_FALSE(IsAnyOverlayOpen());
    }

    TEST_F(ColorWellTests, PaletteSetsTheColor)
    {
        m_Color.A = 0.5f;
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        Settle(Interface());

        // The popover is 216 points wide and hangs 9 points below the well (gap and arrow); its first swatch
        // starts after 12 points of padding and is 20 points large.
        const Vec2 firstSwatch(m_Rect.GetCenter().X - 108.0f + 12.0f + 10.0f,
                               m_Rect.GetBottom() + 9.0f + 12.0f + 10.0f);
        Click(firstSwatch, Interface());
        EXPECT_EQ(m_Changes, 1);
        const Color red = GetTheme().GetColor(StyleColor::Red);
        EXPECT_EQ(m_Color.ToRGBA8(), red.WithAlpha(0.5f).ToRGBA8()) << "the opacity is kept";
        EXPECT_TRUE(IsAnyOverlayOpen()) << "choosing a color leaves the popover open";
    }
} // namespace Carbon
