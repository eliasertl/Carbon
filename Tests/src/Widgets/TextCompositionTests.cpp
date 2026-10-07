#include "Support/WidgetTest.h"

#include <algorithm>
#include <span>
#include <string>
#include <vector>

#include "Carbon/Core/ContextInternal.h"

namespace Carbon
{
    // What the text controls do with an input method's composition: show the pre-edit text inline, leave the
    // keys to the input method, insert what it commits, commit it themselves when the user clicks or moves the
    // focus, and report where the candidate window belongs. CompositionTests covers the input queue.
    class TextCompositionTests : public WidgetTest
    {
    protected:
        Builder Fields()
        {
            return [this]
            {
                BeginVStack({.Spacing = 12.0f, .Padding = 20.0f});
                if (m_IsSecondFieldFirst)
                    m_SecondRect = DrawSecondField();
                m_Changes += TextField("Name", &m_Text, m_Options) ? 1 : 0;
                m_Rect = GetItemRect();
                m_Submits += IsItemSubmitted() ? 1 : 0;
                if (!m_IsSecondFieldFirst)
                    m_SecondRect = DrawSecondField();
                TextArea("Notes", &m_Notes, {.Width = 200.0f, .Height = 80.0f});
                m_AreaRect = GetItemRect();
                EndVStack();
            };
        }

        Rect DrawSecondField()
        {
            TextField("Other", &m_Other);
            return GetItemRect();
        }

        // Clicks at the right end of the field: focused, caret at the end of the text.
        void FocusAtEnd()
        {
            Settle(Fields());
            Click(Vec2(m_Rect.GetRight() - 10.0f, m_Rect.GetCenter().Y), Fields());
            ASSERT_TRUE(GetIO().WantsTextInput());
        }

        void Compose(std::string_view text, size_t caret, std::span<const CompositionClause> clauses = {})
        {
            GetIO().AddCompositionUpdateEvent(text, caret, clauses);
            Frame(Fields());
        }

        void Commit(std::string_view text)
        {
            GetIO().AddCompositionCommitEvent(text);
            Frame(Fields());
        }

        static bool IsComposing() { return Internal::GetContext().Input.Composition.IsActive; }
        static std::string GetComposedText() { return Internal::GetContext().TextEdit.ComposedText; }
        static const Internal::TextEditor& GetEditor() { return Internal::GetContext().TextEdit.Editor; }

        // Where the caret of the field is before byte `offset` of what it shows, by default after all of it.
        float GetCaretX(std::string_view display, size_t offset = std::string_view::npos) const
        {
            std::vector<float> positions;
            GetCaretPositions(display, GetTextSpec(TextStyle::Body), positions);
            return m_Rect.X + 7.0f + positions[std::min(offset, display.size())];
        }

        // Rectangles of the last frame drawn this high, in points: underlines of 1 and 2 points.
        int CountRectsOfHeight(float height) const
        {
            int count = 0;
            for (const DrawPrimitive& primitive : GetDrawData().Primitives)
            {
                if (primitive.Kind == DrawPrimitiveKind::Squircle && primitive.Radius == 0.0f &&
                    primitive.HalfSize.Y == height * 0.5f)
                    count++;
            }
            return count;
        }

        std::string m_Text = "ab";
        std::string m_Other;
        std::string m_Notes;
        TextFieldOptions m_Options;
        Rect m_Rect;
        Rect m_SecondRect;
        Rect m_AreaRect;
        bool m_IsSecondFieldFirst = false;
        int m_Changes = 0;
        int m_Submits = 0;
    };

    // "か" (ka) and "漢字" (kanji), in UTF-8.
    constexpr std::string_view Ka = "\xE3\x81\x8B";
    constexpr std::string_view Kanji = "\xE6\xBC\xA2\xE5\xAD\x97";

    TEST_F(TextCompositionTests, PreEditTextIsShownAtTheCaretButIsNotPartOfTheText)
    {
        FocusAtEnd();
        const int changes = m_Changes;
        Compose(Ka, Ka.size());
        EXPECT_EQ(m_Text, "ab");
        EXPECT_EQ(m_Changes, changes) << "pre-edit text does not change the text";
        EXPECT_EQ(GetComposedText(), std::string("ab") + std::string(Ka));

        // The caret moves inside the pre-edit text; the text's own caret stays where the composition started.
        Compose("xyz", 1);
        EXPECT_EQ(GetComposedText(), "abxyz");
        EXPECT_EQ(GetEditor().GetCaret(), 2u);
        EXPECT_NEAR(GetIO().GetCaretRect().X, GetCaretX("abxyz", 3), 0.01f);
    }

    TEST_F(TextCompositionTests, ACommitInsertsTheTextAtTheCaret)
    {
        FocusAtEnd();
        Click(Vec2(GetCaretX("a") + 0.5f, m_Rect.GetCenter().Y), Fields());
        ASSERT_EQ(GetEditor().GetCaret(), 1u);
        Compose(Ka, Ka.size());
        const int changes = m_Changes;
        Commit(Kanji);
        EXPECT_EQ(m_Text, "a" + std::string(Kanji) + "b");
        EXPECT_EQ(m_Changes, changes + 1);
        EXPECT_EQ(GetEditor().GetCaret(), 1u + Kanji.size());
        EXPECT_FALSE(IsComposing());
        EXPECT_FALSE(GetIO().WantsCompositionCancel()) << "the input method ended this composition itself";

        // One step of undo takes the committed text out again.
        TapKey(Key::LeftCtrl, Key::Z, Fields());
        EXPECT_EQ(m_Text, "ab");
    }

    TEST_F(TextCompositionTests, ACancelLeavesTheTextAsItWas)
    {
        FocusAtEnd();
        Compose(Kanji, Kanji.size());
        GetIO().AddCompositionCancelEvent();
        Frame(Fields());
        EXPECT_EQ(m_Text, "ab");
        EXPECT_FALSE(IsComposing());
    }

    TEST_F(TextCompositionTests, PreEditTextTakesThePlaceOfTheSelection)
    {
        m_Text = "Hello";
        Settle(Fields());
        // Tabbing in selects everything.
        TapKey(Key::Tab, Fields());
        ASSERT_TRUE(GetEditor().HasSelection());

        // An empty composition leaves the selection alone; the first pre-edit character replaces it.
        GetIO().AddCompositionStartEvent();
        Frame(Fields());
        EXPECT_EQ(m_Text, "Hello");
        Compose(Ka, Ka.size());
        EXPECT_EQ(m_Text, "");
        Commit(Ka);
        EXPECT_EQ(m_Text, Ka);
    }

    TEST_F(TextCompositionTests, TheKeysBelongToTheInputMethodWhileItComposes)
    {
        FocusAtEnd();
        Compose(Ka, Ka.size());
        // A host that forwards keys the input method used: nothing happens to the text or the field.
        TapKey(Key::Backspace, Fields());
        TapKey(Key::LeftArrow, Fields());
        TapKey(Key::Enter, Fields());
        TapKey(Key::Escape, Fields());
        EXPECT_EQ(m_Text, "ab");
        EXPECT_EQ(m_Submits, 0);
        EXPECT_TRUE(GetIO().WantsTextInput());
        EXPECT_EQ(GetComposedText(), std::string("ab") + std::string(Ka));

        // Once the composition ends, keys edit the text again.
        Commit(Ka);
        TapKey(Key::Backspace, Fields());
        EXPECT_EQ(m_Text, "ab");
        TapKey(Key::Enter, Fields());
        EXPECT_EQ(m_Submits, 1);
    }

    TEST_F(TextCompositionTests, AClickCommitsThePreEditTextAndAsksTheHostToCancel)
    {
        FocusAtEnd();
        Compose("xyz", 3);
        MoveMouse(Vec2(m_Rect.X + 2.0f, m_Rect.GetCenter().Y), Fields());
        EXPECT_EQ(m_Text, "ab") << "hovering changes nothing";

        PressMouse(Fields());
        EXPECT_EQ(m_Text, "abxyz");
        EXPECT_FALSE(IsComposing());
        EXPECT_TRUE(GetIO().WantsCompositionCancel());
        EXPECT_EQ(GetEditor().GetCaret(), 0u) << "the click acts on the committed text";
        ReleaseMouse(Fields());
        EXPECT_FALSE(GetIO().WantsCompositionCancel());
    }

    TEST_F(TextCompositionTests, LosingTheFocusCommitsThePreEditText)
    {
        FocusAtEnd();
        Compose(Kanji, Kanji.size());
        Click(Vec2(700.0f, 500.0f), Fields());
        EXPECT_EQ(m_Text, "ab" + std::string(Kanji));
        EXPECT_FALSE(IsComposing());
        EXPECT_FALSE(GetIO().WantsTextInput());
    }

    TEST_F(TextCompositionTests, AnotherFieldTakingTheFocusCommitsIntoTheFieldThatWasComposing)
    {
        // Whichever of the two fields comes first in the frame.
        for (const bool isSecondFirst : {false, true})
        {
            m_Text = "ab";
            m_Other.clear();
            m_IsSecondFieldFirst = isSecondFirst;
            FocusAtEnd();
            Compose(Kanji, Kanji.size());

            MoveMouse(m_SecondRect.GetCenter(), Fields());
            PressMouse(Fields());
            const bool wantsCancel = GetIO().WantsCompositionCancel();
            ReleaseMouse(Fields());
            Frame(Fields());
            EXPECT_EQ(m_Text, "ab" + std::string(Kanji)) << "second field first: " << isSecondFirst;
            EXPECT_TRUE(m_Other.empty()) << "second field first: " << isSecondFirst;
            EXPECT_FALSE(IsComposing());
            EXPECT_TRUE(wantsCancel);

            // The second field edits on its own.
            Commit("x");
            EXPECT_EQ(m_Other, "x");
            Click(Vec2(700.0f, 500.0f), Fields());
        }
    }

    TEST_F(TextCompositionTests, TheCaretRectFollowsTheCaretAndTheActiveClause)
    {
        Settle(Fields());
        EXPECT_TRUE(GetIO().GetCaretRect().IsEmpty()) << "nothing is edited";

        FocusAtEnd();
        Rect caret = GetIO().GetCaretRect();
        EXPECT_NEAR(caret.X, GetCaretX("ab"), 0.01f);
        EXPECT_GT(caret.Height, 10.0f);
        EXPECT_GE(caret.Y, m_Rect.Y);
        EXPECT_LE(caret.GetBottom(), m_Rect.GetBottom());

        // The candidate window belongs under the clause being converted, not at the caret.
        const CompositionClause clauses[] = {{0, 1, false}, {1, 3, true}};
        Compose("xyz", 3, clauses);
        caret = GetIO().GetCaretRect();
        EXPECT_NEAR(caret.X, GetCaretX("abxyz", 3), 0.01f);
        Compose("xyz", 2);
        EXPECT_NEAR(GetIO().GetCaretRect().X, GetCaretX("abxyz", 4), 0.01f);
    }

    TEST_F(TextCompositionTests, ClausesAreUnderlinedAndTheActiveOneMoreStrongly)
    {
        FocusAtEnd();
        Compose("xyz", 3);
        EXPECT_EQ(CountRectsOfHeight(1.0f), 1) << "one thin line under the whole pre-edit text";
        EXPECT_EQ(CountRectsOfHeight(2.0f), 0);

        const CompositionClause clauses[] = {{0, 1, false}, {1, 3, true}};
        Compose("xyz", 3, clauses);
        EXPECT_EQ(CountRectsOfHeight(1.0f), 1);
        EXPECT_EQ(CountRectsOfHeight(2.0f), 1);

        Commit("xyz");
        EXPECT_EQ(CountRectsOfHeight(1.0f) + CountRectsOfHeight(2.0f), 0);
    }

    TEST_F(TextCompositionTests, ASecureFieldShowsOnlyCommittedText)
    {
        m_Options.IsSecure = true;
        FocusAtEnd();
        GetIO().AddCompositionUpdateEvent("xyz", 3);
        Frame(Fields());
        EXPECT_EQ(CountRectsOfHeight(1.0f), 0);
        EXPECT_EQ(Internal::GetContext().TextEdit.SecureText, "\xE2\x80\xA2\xE2\x80\xA2");
        Commit("xyz");
        EXPECT_EQ(m_Text, "abxyz");
    }

    TEST_F(TextCompositionTests, TheTextAreaLaysOutThePreEditTextWithItsLines)
    {
        m_Notes = "first\nsecond";
        Settle(Fields());
        const float lineHeight = GetFontMetrics(GetTextSpec(TextStyle::Body)).LineHeight;
        // The end of the first line.
        Click(m_AreaRect.GetMin() + Vec2(150.0f, 4.0f + lineHeight * 0.5f), Fields());
        ASSERT_EQ(GetEditor().GetCaret(), 5u);

        Compose("xyz", 3);
        EXPECT_EQ(m_Notes, "first\nsecond");
        EXPECT_EQ(GetComposedText(), "firstxyz\nsecond");
        const std::vector<Internal::TextAreaLine>& lines = Internal::GetContext().TextEdit.AreaLines;
        ASSERT_EQ(lines.size(), 2u);
        EXPECT_EQ(lines[0].End, 8u);
        const Rect caret = GetIO().GetCaretRect();
        EXPECT_LT(caret.Y, m_AreaRect.Y + 4.0f + lineHeight * 0.5f) << "the caret rect is on the first line";
        EXPECT_EQ(CountRectsOfHeight(1.0f), 1);

        // Keys are the input method's: Enter does not break the line.
        TapKey(Key::Enter, Fields());
        EXPECT_EQ(m_Notes, "first\nsecond");

        Commit("xyz");
        EXPECT_EQ(m_Notes, "firstxyz\nsecond");
        TapKey(Key::Enter, Fields());
        EXPECT_EQ(m_Notes, "firstxyz\n\nsecond");
    }

    TEST_F(TextCompositionTests, PreEditTextThatWrapsIsUnderlinedOnEachLine)
    {
        Settle(Fields());
        Click(m_AreaRect.GetCenter(), Fields());
        // Longer than the area is wide: the words wrap, and so does the underline.
        Compose("pre edit text long enough to wrap onto a second line", 0);
        ASSERT_GE(Internal::GetContext().TextEdit.AreaLines.size(), 2u);
        EXPECT_GE(CountRectsOfHeight(1.0f), 2);
        Commit("done");
        EXPECT_EQ(m_Notes, "done");
    }
} // namespace Carbon
