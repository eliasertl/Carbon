#include "Support/WidgetTest.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>

#include "Carbon/Extensions/Extensions.h"

namespace Carbon
{
    // ---- Formatting and parsing ----------------------------------------------------------------------------------

    std::string Format(double value, const NumberFormat& format = {})
    {
        std::array<char, 64> buffer;
        return std::string(FormatNumber(value, format, buffer));
    }

    TEST(NumberFormatTests, AutomaticDecimalsDropTrailingZeros)
    {
        EXPECT_EQ(Format(3.0), "3");
        EXPECT_EQ(Format(2.5), "2.5");
        EXPECT_EQ(Format(1.0 / 3.0), "0.333");
        EXPECT_EQ(Format(-1234.5678), "-1234.568");
        EXPECT_EQ(Format(-0.0001), "0") << "a negative number that rounds to zero has no sign";
        EXPECT_EQ(Format(1e20), "100000000000000000000");
    }

    TEST(NumberFormatTests, FixedDecimalsPrefixAndSuffix)
    {
        EXPECT_EQ(Format(3.0, {.Decimals = 2}), "3.00");
        EXPECT_EQ(Format(2.345, {.Decimals = 0}), "2");
        EXPECT_EQ(Format(12.0, {.Prefix = "$"}), "$12");
        EXPECT_EQ(Format(4.5, {.Decimals = 1, .Suffix = " px"}), "4.5 px");
        EXPECT_EQ(Format(std::numeric_limits<double>::infinity()), "\xE2\x88\x9E");
    }

    TEST(NumberFormatTests, FormattingStopsAtTheEndOfTheBufferWithoutSplittingACharacter)
    {
        std::array<char, 5> buffer;
        // "12 €": the euro sign takes three bytes, which do not fit after "12 ".
        EXPECT_EQ(FormatNumber(12.0, {.Suffix = " \xE2\x82\xAC"}, buffer), "12 ");
        EXPECT_EQ(FormatNumber(123456.0, {}, buffer), "12345");
    }

    TEST(NumberFormatTests, ParsingAcceptsWhatPeopleType)
    {
        double value = 0.0;
        EXPECT_TRUE(ParseNumber(" 42 ", {}, &value));
        EXPECT_DOUBLE_EQ(value, 42.0);
        EXPECT_TRUE(ParseNumber("+1.5", {}, &value));
        EXPECT_DOUBLE_EQ(value, 1.5);
        EXPECT_TRUE(ParseNumber("-2,25", {}, &value)) << "a single comma is a decimal point";
        EXPECT_DOUBLE_EQ(value, -2.25);
        EXPECT_TRUE(ParseNumber("1e3", {}, &value));
        EXPECT_DOUBLE_EQ(value, 1000.0);
        EXPECT_TRUE(ParseNumber("$ 12", {.Prefix = "$"}, &value));
        EXPECT_DOUBLE_EQ(value, 12.0);
        EXPECT_TRUE(ParseNumber("12px", {.Suffix = " px"}, &value));
        EXPECT_DOUBLE_EQ(value, 12.0);
        EXPECT_TRUE(ParseNumber("7", {.Suffix = " px"}, &value)) << "the suffix may be left out";
        EXPECT_DOUBLE_EQ(value, 7.0);
    }

    TEST(NumberFormatTests, ParsingRejectsWhatIsNotANumber)
    {
        for (const char* text : {"", "  ", "abc", "1.2.3", "1,2,3", "12 apples", "inf", "nan", "--1", "1e999"})
        {
            double value = 5.0;
            EXPECT_FALSE(ParseNumber(text, {}, &value)) << text;
            EXPECT_DOUBLE_EQ(value, 5.0) << text;
        }
    }

    // ---- NumberField ---------------------------------------------------------------------------------------------

    template <typename T>
    class NumberControlTest : public WidgetTest
    {
    protected:
        Builder Field()
        {
            return [this]
            {
                m_Changes += NumberField("Amount", &m_Value, m_Options) ? 1 : 0;
                m_Rect = GetItemRect();
                // Somewhere for Tab to go.
                Button("Next");
            };
        }

        void Focus()
        {
            Settle(Field());
            Click(m_Rect.GetCenter(), Field());
            ASSERT_EQ(GetFocusedID(), GetID("Amount"));
        }

        // Replaces the field's text: select everything, then type.
        void Retype(std::string_view text)
        {
            TapKey(Key::LeftCtrl, Key::A, Field());
            Type(text, Field());
        }

        T m_Value = T(10);
        NumberFieldOptions m_Options;
        int m_Changes = 0;
        Rect m_Rect;
    };

    using NumberFieldDoubleTests = NumberControlTest<double>;
    using NumberFieldFloatTests = NumberControlTest<float>;
    using NumberFieldIntTests = NumberControlTest<int>;

    TEST_F(NumberFieldDoubleTests, TypingIsAppliedOnEnter)
    {
        Focus();
        Retype("12.5");
        EXPECT_DOUBLE_EQ(m_Value, 10.0) << "nothing is applied while typing";
        EXPECT_EQ(m_Changes, 0);
        TapKey(Key::Enter, Field());
        EXPECT_DOUBLE_EQ(m_Value, 12.5);
        EXPECT_EQ(m_Changes, 1);
        EXPECT_TRUE(IsFocused(GetID("Amount"))) << "Enter keeps the focus, as in a text field";
    }

    TEST_F(NumberFieldDoubleTests, TypingIsAppliedWhenTheFieldLosesFocus)
    {
        Focus();
        Retype("3");
        TapKey(Key::Tab, Field());
        EXPECT_DOUBLE_EQ(m_Value, 3.0);
        EXPECT_EQ(m_Changes, 1);
    }

    TEST_F(NumberFieldDoubleTests, InvalidInputLeavesTheValue)
    {
        Focus();
        Retype("twelve");
        TapKey(Key::Enter, Field());
        EXPECT_DOUBLE_EQ(m_Value, 10.0);
        EXPECT_EQ(m_Changes, 0);

        // The field shows the value again, so typing on starts from a number.
        TextFieldSelection selection;
        ASSERT_TRUE(GetTextFieldSelection("Amount", &selection));
        EXPECT_EQ(selection.Caret, 2u) << "the text is \"10\" again, with the caret at its end";
        TapKey(Key::UpArrow, Field());
        EXPECT_DOUBLE_EQ(m_Value, 11.0);
    }

    TEST_F(NumberFieldDoubleTests, EscapeDiscardsTheTypingAndLeaves)
    {
        Focus();
        Retype("99");
        TapKey(Key::Escape, Field());
        EXPECT_DOUBLE_EQ(m_Value, 10.0);
        EXPECT_EQ(m_Changes, 0);
        EXPECT_FALSE(IsFocused(GetID("Amount")));

        // Focusing again shows the value, not the discarded text.
        Click(m_Rect.GetCenter(), Field());
        TapKey(Key::Enter, Field());
        EXPECT_DOUBLE_EQ(m_Value, 10.0);
    }

    TEST_F(NumberFieldDoubleTests, ArrowsStepWithFineAndCoarseModifiers)
    {
        m_Options.Step = 1.0;
        Focus();
        TapKey(Key::UpArrow, Field());
        EXPECT_DOUBLE_EQ(m_Value, 11.0);
        TapKey(Key::DownArrow, Field());
        TapKey(Key::DownArrow, Field());
        EXPECT_DOUBLE_EQ(m_Value, 9.0);
        TapKey(Key::LeftShift, Key::UpArrow, Field());
        EXPECT_NEAR(m_Value, 9.1, 1e-9);
        TapKey(Key::LeftCtrl, Key::UpArrow, Field());
        EXPECT_NEAR(m_Value, 19.1, 1e-9);
        EXPECT_EQ(m_Changes, 5);

        // A step goes from the typed number.
        Retype("50");
        TapKey(Key::UpArrow, Field());
        EXPECT_DOUBLE_EQ(m_Value, 51.0);
    }

    TEST_F(NumberFieldDoubleTests, ValuesAreClampedToTheRange)
    {
        m_Options.Min = 0.0;
        m_Options.Max = 20.0;
        Focus();
        Retype("50");
        TapKey(Key::Enter, Field());
        EXPECT_DOUBLE_EQ(m_Value, 20.0);
        TapKey(Key::UpArrow, Field());
        EXPECT_DOUBLE_EQ(m_Value, 20.0);
        EXPECT_EQ(m_Changes, 1) << "stepping at the end changes nothing";
        Retype("-3");
        TapKey(Key::Enter, Field());
        EXPECT_DOUBLE_EQ(m_Value, 0.0);
    }

    TEST_F(NumberFieldDoubleTests, TheFieldFollowsTheValueUntilTheUserTypes)
    {
        Focus();
        m_Value = 42.0;
        Frame(Field());
        TapKey(Key::UpArrow, Field());
        EXPECT_DOUBLE_EQ(m_Value, 43.0);

        // Typed text is kept while the application changes the value, and wins on Enter.
        Retype("7");
        m_Value = 100.0;
        Frame(Field());
        TapKey(Key::Enter, Field());
        EXPECT_DOUBLE_EQ(m_Value, 7.0);
    }

    TEST_F(NumberFieldIntTests, IntegersAreRounded)
    {
        Focus();
        Retype("3.6");
        TapKey(Key::Enter, Field());
        EXPECT_EQ(m_Value, 4);
        TapKey(Key::LeftShift, Key::UpArrow, Field());
        EXPECT_EQ(m_Value, 4) << "a tenth of a step rounds away";
        TapKey(Key::UpArrow, Field());
        EXPECT_EQ(m_Value, 5);
        Retype("1e12");
        TapKey(Key::Enter, Field());
        EXPECT_EQ(m_Value, std::numeric_limits<int>::max()) << "clamped to what an int holds";
    }

    TEST_F(NumberFieldFloatTests, FloatsWork)
    {
        m_Options.Step = 0.5;
        m_Options.Format.Decimals = 1;
        Focus();
        TapKey(Key::UpArrow, Field());
        EXPECT_FLOAT_EQ(m_Value, 10.5f);
        Retype("0.1");
        TapKey(Key::Enter, Field());
        EXPECT_FLOAT_EQ(m_Value, 0.1f);
        EXPECT_EQ(m_Changes, 2);
    }

    TEST_F(NumberFieldDoubleTests, AnEmptyRangeIsReported)
    {
        m_Options.Min = 5.0;
        m_Options.Max = 1.0;
        Frame(Field());
        EXPECT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_DOUBLE_EQ(m_Value, 10.0);
    }

    // ---- ScrubField ----------------------------------------------------------------------------------------------

    template <typename T>
    class ScrubFieldTest : public WidgetTest
    {
    protected:
        Builder Field()
        {
            return [this]
            {
                m_Changes += ScrubField("Size", &m_Value, m_Options) ? 1 : 0;
                m_Rect = GetItemRect();
                Button("Next");
            };
        }

        // Presses at the field's center, moves by `distance` points in steps of `step`, and releases.
        void Drag(float distance, float step = 5.0f)
        {
            Settle(Field());
            const Vec2 start = m_Rect.GetCenter();
            MoveMouse(start, Field());
            PressMouse(Field());
            float moved = 0.0f;
            while (std::abs(moved) < std::abs(distance))
            {
                moved += std::copysign(std::min(step, std::abs(distance) - std::abs(moved)), distance);
                MoveMouse(start + Vec2(moved, 0.0f), Field());
            }
            ReleaseMouse(Field());
        }

        T m_Value = T(10);
        ScrubFieldOptions m_Options;
        int m_Changes = 0;
        Rect m_Rect;
    };

    using ScrubFieldDoubleTests = ScrubFieldTest<double>;
    using ScrubFieldIntTests = ScrubFieldTest<int>;
    using ScrubFieldFloatTests = ScrubFieldTest<float>;

    TEST_F(ScrubFieldIntTests, DraggingChangesTheValueByStepPerPoint)
    {
        // The first 3 points only tell a drag from a click.
        Drag(23.0f);
        EXPECT_EQ(m_Value, 30);
        Drag(-13.0f);
        EXPECT_EQ(m_Value, 20);
        EXPECT_GT(m_Changes, 2);
        EXPECT_FALSE(IsFocused(GetID("Size"))) << "a drag does not start typing";
    }

    TEST_F(ScrubFieldDoubleTests, ShiftIsFineAndCtrlIsCoarse)
    {
        GetIO().AddKeyEvent(Key::LeftShift, true);
        Drag(23.0f);
        EXPECT_NEAR(m_Value, 12.0, 1e-9);
        GetIO().AddKeyEvent(Key::LeftShift, false);
        // Two points at ten per point from 12 make 32; values from dragging are multiples of the step in use.
        GetIO().AddKeyEvent(Key::LeftCtrl, true);
        Drag(5.0f);
        EXPECT_NEAR(m_Value, 30.0, 1e-9);
        GetIO().AddKeyEvent(Key::LeftCtrl, false);
    }

    TEST_F(ScrubFieldFloatTests, SlowDragsAddUpAndStayInRange)
    {
        m_Options.Step = 0.01;
        m_Options.Min = 0.0;
        m_Options.Max = 10.5;
        // One point at a time: each moves the value by a hundredth.
        Drag(53.0f, 1.0f);
        EXPECT_NEAR(m_Value, 10.5f, 1e-5f);
        Drag(-103.0f, 1.0f);
        EXPECT_NEAR(m_Value, 9.5f, 1e-5f) << "the drag starts again from the clamped value";
    }

    TEST_F(ScrubFieldDoubleTests, AClickTurnsItIntoAFieldWithTheValueSelected)
    {
        Settle(Field());
        Click(m_Rect.GetCenter(), Field());
        Frame(Field());
        EXPECT_TRUE(IsFocused(GetID("Size")));
        TextFieldSelection selection;
        ASSERT_TRUE(GetTextFieldSelection("Size", &selection));
        EXPECT_EQ(selection.Start, 0u);
        EXPECT_EQ(selection.End, 2u) << "all of \"10\"";

        Type("64", Field());
        TapKey(Key::Enter, Field());
        EXPECT_DOUBLE_EQ(m_Value, 64.0);

        // Escape leaves typing; dragging works again.
        TapKey(Key::Escape, Field());
        EXPECT_FALSE(IsFocused(GetID("Size")));
        Drag(13.0f);
        EXPECT_DOUBLE_EQ(m_Value, 74.0);
    }

    TEST_F(ScrubFieldIntTests, TabStartsTypingWithTheValueSelected)
    {
        Settle(Field());
        TapKey(Key::Tab, Field());
        EXPECT_TRUE(IsFocused(GetID("Size")));
        Type("7", Field());
        TapKey(Key::UpArrow, Field());
        EXPECT_EQ(m_Value, 8);
        TapKey(Key::Tab, Field());
        EXPECT_FALSE(IsFocused(GetID("Size")));
    }
} // namespace Carbon
