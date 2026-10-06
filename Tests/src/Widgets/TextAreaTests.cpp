#include "Support/WidgetTest.h"

#include <algorithm>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace Carbon
{
    class TextAreaTests : public WidgetTest
    {
    protected:
        void SetUp() override
        {
            WidgetTest::SetUp();
            // A host with a clipboard.
            DestroyContext(m_Context);
            ContextDescription description;
            description.Callbacks.AssertFailed = [this](const AssertInfo& info)
            { m_AssertMessages.emplace_back(info.Message); };
            description.Callbacks.GetClipboardText = [this] { return m_Clipboard; };
            description.Callbacks.SetClipboardText = [this](std::string_view text) { m_Clipboard = std::string(text); };
            m_Context = CreateContext(description);
            SetCurrentContext(m_Context);
            GetIO().SetDisplaySize(800.0f, 600.0f);
        }

        Builder Area()
        {
            return [this]
            {
                m_Changes += TextArea("Notes", &m_Text, m_Options) ? 1 : 0;
                m_Rect = GetItemRect();
                m_LineHeight = GetFontMetrics(GetTextSpec(TextStyle::Body)).LineHeight;
                // Somewhere for Tab to go.
                Button("Next");
            };
        }

        // The top-left corner of the text inside the area.
        Vec2 GetTextOrigin() const { return m_Rect.GetMin() + Vec2(6.0f, 4.0f); }

        // A point in the middle of line `line` (from 0), `x` points from the start of the text.
        Vec2 GetLinePoint(int line, float x) const
        {
            return GetTextOrigin() + Vec2(x, m_LineHeight * (static_cast<float>(line) + 0.5f));
        }

        void Focus()
        {
            Settle(Area());
            Click(GetLinePoint(0, 1.0f), Area());
            ASSERT_EQ(GetFocusedID(), GetID("Notes"));
        }

        TextFieldSelection GetSelection()
        {
            TextFieldSelection selection;
            EXPECT_TRUE(GetTextFieldSelection("Notes", &selection));
            return selection;
        }

        size_t GetCaret() { return GetSelection().Caret; }

        std::string m_Text;
        TextAreaOptions m_Options;
        Rect m_Rect;
        float m_LineHeight = 16.0f;
        int m_Changes = 0;
        std::string m_Clipboard;
    };

    TEST_F(TextAreaTests, HasAFixedDefaultSizeAndGrowsWhenItFits)
    {
        Settle(Area());
        EXPECT_EQ(m_Rect.GetSize(), Vec2(240.0f, 96.0f));

        m_Text = "one\ntwo\nthree";
        m_Options.Height = Size::Fit();
        Settle(Area());
        EXPECT_FLOAT_EQ(m_Rect.Height, m_LineHeight * 3.0f + 8.0f);
    }

    TEST_F(TextAreaTests, TypingReturnAndBackspaceEditLines)
    {
        Focus();
        EXPECT_TRUE(GetIO().WantsTextInput());
        Type("Hello", Area());
        TapKey(Key::Enter, Area());
        Type("World", Area());
        EXPECT_EQ(m_Text, "Hello\nWorld");
        EXPECT_EQ(m_Changes, 3);
        EXPECT_FALSE(IsItemSubmitted()) << "Return makes a new line; it does not submit";

        // Backspace at the start of a line joins it to the line before.
        TapKey(Key::Home, Area());
        TapKey(Key::Backspace, Area());
        EXPECT_EQ(m_Text, "HelloWorld");
        TapKey(Key::LeftCtrl, Key::Z, Area());
        EXPECT_EQ(m_Text, "Hello\nWorld");
    }

    TEST_F(TextAreaTests, LongLinesWrapAtWordsInsideTheWidth)
    {
        m_Text = "alpha beta gamma delta epsilon zeta eta theta";
        m_Options.Width = 100.0f;
        m_Options.Height = Size::Fit();
        Settle(Area());
        const int lines = static_cast<int>(std::lround((m_Rect.Height - 8.0f) / m_LineHeight));
        EXPECT_GE(lines, 3);

        // Each visual line ends after a space: End stops before it, and Down goes to the next word.
        Click(GetLinePoint(0, 1.0f), Area());
        TapKey(Key::Home, Area());
        TapKey(Key::End, Area());
        const size_t end = GetCaret();
        EXPECT_EQ(m_Text[end], ' ') << "End of a wrapped line is before the space where it wrapped";
        TapKey(Key::Home, Area());
        TapKey(Key::DownArrow, Area());
        const size_t secondLine = GetCaret();
        EXPECT_EQ(secondLine, end + 1);
        EXPECT_EQ(m_Text[secondLine - 1], ' ');

        // A wide area needs one line.
        m_Options.Width = 1000.0f;
        Settle(Area());
        EXPECT_FLOAT_EQ(m_Rect.Height, m_LineHeight + 8.0f);
    }

    TEST_F(TextAreaTests, AWordLongerThanTheLineBreaksAnywhere)
    {
        m_Text = std::string(60, 'W');
        m_Options.Width = 100.0f;
        m_Options.Height = Size::Fit();
        Settle(Area());
        EXPECT_GE(m_Rect.Height, m_LineHeight * 3.0f + 8.0f);
    }

    TEST_F(TextAreaTests, UpAndDownKeepToOneColumn)
    {
        m_Text = "abcdef\nab\nabcdef";
        Focus();
        TapKey(Key::LeftCtrl, Key::Home, Area());
        TapKey(Key::End, Area());
        EXPECT_EQ(GetCaret(), 6u);
        TapKey(Key::DownArrow, Area());
        EXPECT_EQ(GetCaret(), 9u) << "the end of the short line";
        TapKey(Key::DownArrow, Area());
        EXPECT_EQ(GetCaret(), 16u) << "back in the column it came from";
        TapKey(Key::DownArrow, Area());
        EXPECT_EQ(GetCaret(), m_Text.size()) << "below the last line is the end";
        TapKey(Key::UpArrow, Area());
        TapKey(Key::UpArrow, Area());
        TapKey(Key::UpArrow, Area());
        TapKey(Key::UpArrow, Area());
        EXPECT_EQ(GetCaret(), 0u) << "above the first line is the start";

        // Shift selects across lines.
        TapKey(Key::LeftShift, Key::DownArrow, Area());
        EXPECT_EQ(GetSelection().Start, 0u);
        EXPECT_EQ(GetSelection().End, 7u);
    }

    TEST_F(TextAreaTests, HomeAndEndWorkOnLinesAndWithCtrlOnTheText)
    {
        m_Text = "first\nsecond\nthird";
        Focus();
        TapKey(Key::DownArrow, Area());
        TapKey(Key::End, Area());
        EXPECT_EQ(GetCaret(), 12u);
        TapKey(Key::Home, Area());
        EXPECT_EQ(GetCaret(), 6u);
        TapKey(Key::LeftCtrl, Key::End, Area());
        EXPECT_EQ(GetCaret(), m_Text.size());
        TapKey(Key::LeftCtrl, Key::Home, Area());
        EXPECT_EQ(GetCaret(), 0u);
    }

    TEST_F(TextAreaTests, ClipboardKeepsLineBreaks)
    {
        m_Text = "one\ntwo";
        Focus();
        TapKey(Key::LeftCtrl, Key::A, Area());
        TapKey(Key::LeftCtrl, Key::C, Area());
        EXPECT_EQ(m_Clipboard, "one\ntwo");

        m_Clipboard = "a\r\nb\rc\td";
        TapKey(Key::LeftCtrl, Key::V, Area());
        EXPECT_EQ(m_Text, "a\nb\nc\td") << "Windows and old Mac line breaks become \\n; tabs stay";
    }

    TEST_F(TextAreaTests, TabMovesTheFocusUnlessTheAreaAcceptsTabs)
    {
        Focus();
        TapKey(Key::Tab, Area());
        EXPECT_EQ(GetFocusedID(), GetID("Next"));
        EXPECT_EQ(m_Text, "");

        m_Options.AcceptsTab = true;
        Click(GetLinePoint(0, 1.0f), Area());
        Type("a", Area());
        TapKey(Key::Tab, Area());
        Type("b", Area());
        EXPECT_EQ(m_Text, "a\tb");
        EXPECT_EQ(GetFocusedID(), GetID("Notes"));

        // Ctrl+Tab leaves the area, as in a macOS text view.
        TapKey(Key::LeftCtrl, Key::Tab, Area());
        EXPECT_EQ(GetFocusedID(), GetID("Next"));
        EXPECT_EQ(m_Text, "a\tb");
    }

    TEST_F(TextAreaTests, EscapeGivesUpFocus)
    {
        Focus();
        TapKey(Key::Escape, Area());
        EXPECT_NE(GetFocusedID(), GetID("Notes"));
        EXPECT_FALSE(GetIO().WantsTextInput());
    }

    TEST_F(TextAreaTests, ClicksPlaceTheCaretOnTheirLineAndDragsSelectAcrossLines)
    {
        m_Text = "first line\nsecond line\nthird line";
        Settle(Area());
        Click(GetLinePoint(1, 0.5f), Area());
        EXPECT_EQ(GetCaret(), 11u) << "the start of the second line";

        // Double click selects a word.
        Click(GetLinePoint(2, 10.0f), Area());
        PressMouse(Area());
        ReleaseMouse(Area());
        EXPECT_EQ(GetSelection().Start, 23u);
        EXPECT_EQ(GetSelection().End, 28u) << "\"third\"";

        // Drag from the first line into the third.
        MoveMouse(GetLinePoint(0, 0.5f), Area());
        Frame(Area(), 1.0f); // past the double-click interval
        PressMouse(Area());
        MoveMouse(GetLinePoint(2, 0.5f), Area());
        ReleaseMouse(Area());
        EXPECT_EQ(GetSelection().Start, 0u);
        EXPECT_EQ(GetSelection().End, 23u);
    }

    TEST_F(TextAreaTests, LongTextScrollsToKeepTheCaretVisible)
    {
        for (int i = 0; i < 20; i++)
            m_Text += "line " + std::to_string(i) + "\n";
        m_Options.Height = 4.0f * 16.0f + 8.0f;
        Focus();
        const float visible = m_Rect.Height - 8.0f;

        TapKey(Key::LeftCtrl, Key::End, Area());
        Settle(Area());
        const float bottom = m_LineHeight * 21.0f; // 20 lines and the empty one after the last line break
        EXPECT_FLOAT_EQ(GetScrollOffset("Notes").Y, bottom - visible);

        // Only the lines in view are drawn: far fewer quads than the 20 lines' glyphs.
        const size_t quads = GetDrawData().Vertices.size() / 4;
        EXPECT_LT(quads, 50u);

        TapKey(Key::LeftCtrl, Key::Home, Area());
        Settle(Area());
        EXPECT_FLOAT_EQ(GetScrollOffset("Notes").Y, 0.0f);

        // The wheel scrolls the area under the pointer, and the caret stays where it was.
        GetIO().AddMouseWheelEvent(0.0f, -1.0f);
        Settle(Area());
        EXPECT_GT(GetScrollOffset("Notes").Y, 0.0f);
        EXPECT_EQ(GetCaret(), 0u);
    }

    TEST_F(TextAreaTests, MaxLengthCountsLineBreaks)
    {
        m_Options.MaxLength = 4;
        Focus();
        Type("ab", Area());
        TapKey(Key::Enter, Area());
        Type("cdef", Area());
        EXPECT_EQ(m_Text, "ab\nc");
    }

    TEST_F(TextAreaTests, BufferFormStaysInsideTheBuffer)
    {
        char storage[12];
        std::fill(std::begin(storage), std::end(storage), '#');
        std::memcpy(storage, "ab", 3);
        const Builder area = [&]
        {
            TextArea("Notes", std::span<char>(storage, 8));
            m_Rect = GetItemRect();
            m_LineHeight = GetFontMetrics(GetTextSpec(TextStyle::Body)).LineHeight;
        };
        Settle(area);
        Click(GetLinePoint(0, 100.0f), area);
        TapKey(Key::Enter, area);
        Type("cdefgh", area);
        EXPECT_STREQ(storage, "ab\ncdef");
        EXPECT_TRUE(std::all_of(storage + 8, storage + 12, [](char c) { return c == '#'; }));
    }

    TEST_F(TextAreaTests, CallbackFormReportsChanges)
    {
        std::string stored = "x";
        int calls = 0;
        const Builder area = [&]
        {
            TextArea("Notes", stored,
                     [&](std::string_view text)
                     {
                         stored = text;
                         calls++;
                     });
            m_Rect = GetItemRect();
            m_LineHeight = GetFontMetrics(GetTextSpec(TextStyle::Body)).LineHeight;
        };
        Settle(area);
        EXPECT_EQ(calls, 0);
        Click(GetLinePoint(0, 100.0f), area);
        TapKey(Key::Enter, area);
        Type("y", area);
        EXPECT_EQ(stored, "x\ny");
        EXPECT_EQ(calls, 2);
    }

    TEST_F(TextAreaTests, ADisabledAreaIgnoresInput)
    {
        m_Text = "fixed";
        m_Options.Disabled = true;
        Settle(Area());
        Click(GetLinePoint(0, 1.0f), Area());
        Type("x", Area());
        EXPECT_EQ(m_Text, "fixed");
        EXPECT_NE(GetFocusedID(), GetID("Notes"));
    }
} // namespace Carbon
