#include "Support/WidgetTest.h"

#include <algorithm>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Widgets/Internal/TextEditor.h"

namespace Carbon
{
    // ---- The editing logic, without any UI -------------------------------------------------------------------

    class TextEditorTests : public ::testing::Test
    {
    protected:
        void Start(const std::string& text)
        {
            m_Text = text;
            m_Editor.Reset(m_Text);
        }

        std::string m_Text;
        Internal::TextEditor m_Editor;
    };

    TEST_F(TextEditorTests, StartsWithTheCaretAtTheEnd)
    {
        Start("Hello");
        EXPECT_EQ(m_Editor.GetCaret(), 5u);
        EXPECT_FALSE(m_Editor.HasSelection());
    }

    TEST_F(TextEditorTests, InsertsAtTheCaret)
    {
        Start("Hello");
        EXPECT_TRUE(m_Editor.Insert(m_Text, " world"));
        EXPECT_EQ(m_Text, "Hello world");
        EXPECT_EQ(m_Editor.GetCaret(), 11u);

        m_Editor.SetCaret(m_Text, 0, false);
        m_Editor.Insert(m_Text, ">> ");
        EXPECT_EQ(m_Text, ">> Hello world");
        EXPECT_EQ(m_Editor.GetCaret(), 3u);
    }

    TEST_F(TextEditorTests, TypingReplacesTheSelection)
    {
        Start("Hello world");
        m_Editor.SetCaret(m_Text, 6, false);
        m_Editor.SetCaret(m_Text, 11, true); // select "world"
        EXPECT_EQ(m_Editor.GetSelectedText(m_Text), "world");
        m_Editor.Insert(m_Text, "there");
        EXPECT_EQ(m_Text, "Hello there");
        EXPECT_FALSE(m_Editor.HasSelection());
        EXPECT_EQ(m_Editor.GetCaret(), 11u);
    }

    TEST_F(TextEditorTests, MovesOverWholeCharacters)
    {
        Start("a\xC3\xA4\xE2\x82\xAC"); // a, ä (2 bytes), € (3 bytes)
        m_Editor.MoveLeft(m_Text, false, false);
        EXPECT_EQ(m_Editor.GetCaret(), 3u);
        m_Editor.MoveLeft(m_Text, false, false);
        EXPECT_EQ(m_Editor.GetCaret(), 1u);
        m_Editor.MoveLeft(m_Text, false, false);
        m_Editor.MoveLeft(m_Text, false, false); // already at the start
        EXPECT_EQ(m_Editor.GetCaret(), 0u);
        m_Editor.MoveRight(m_Text, false, false);
        m_Editor.MoveRight(m_Text, false, false);
        EXPECT_EQ(m_Editor.GetCaret(), 3u);
        m_Editor.MoveToStart(m_Text, false);
        EXPECT_EQ(m_Editor.GetCaret(), 0u);
        m_Editor.MoveToEnd(m_Text, false);
        EXPECT_EQ(m_Editor.GetCaret(), 6u);
    }

    TEST_F(TextEditorTests, ShiftExtendsAndPlainArrowsCollapse)
    {
        Start("abcdef");
        m_Editor.SetCaret(m_Text, 2, false);
        m_Editor.MoveRight(m_Text, true, false);
        m_Editor.MoveRight(m_Text, true, false);
        EXPECT_EQ(m_Editor.GetSelectedText(m_Text), "cd");
        EXPECT_EQ(m_Editor.GetAnchor(), 2u);

        // Left without Shift collapses to the start of the selection; right, to its end.
        m_Editor.MoveLeft(m_Text, false, false);
        EXPECT_FALSE(m_Editor.HasSelection());
        EXPECT_EQ(m_Editor.GetCaret(), 2u);

        m_Editor.MoveToEnd(m_Text, true);
        EXPECT_EQ(m_Editor.GetSelectedText(m_Text), "cdef");
        m_Editor.MoveRight(m_Text, false, false);
        EXPECT_EQ(m_Editor.GetCaret(), 6u);
        EXPECT_FALSE(m_Editor.HasSelection());

        m_Editor.SelectAll(m_Text);
        EXPECT_EQ(m_Editor.GetSelectedText(m_Text), "abcdef");
    }

    TEST_F(TextEditorTests, WordMovementAndSelection)
    {
        Start("open the pod-bay doors");
        m_Editor.MoveLeft(m_Text, false, true);
        EXPECT_EQ(m_Editor.GetCaret(), 17u); // before "doors"
        m_Editor.MoveLeft(m_Text, false, true);
        EXPECT_EQ(m_Editor.GetCaret(), 13u); // before "bay"; punctuation separates words
        m_Editor.MoveToStart(m_Text, false);
        m_Editor.MoveRight(m_Text, false, true);
        EXPECT_EQ(m_Editor.GetCaret(), 4u); // after "open"
        m_Editor.MoveRight(m_Text, false, true);
        EXPECT_EQ(m_Editor.GetCaret(), 8u); // after "the"

        m_Editor.SelectWordAt(m_Text, 6); // inside "the"
        EXPECT_EQ(m_Editor.GetSelectedText(m_Text), "the");
        m_Editor.SelectWordAt(m_Text, 4); // on the space after "open"
        EXPECT_EQ(m_Editor.GetSelectedText(m_Text), " ");
        m_Editor.SelectWordAt(m_Text, m_Text.size()); // at the very end: the last word
        EXPECT_EQ(m_Editor.GetSelectedText(m_Text), "doors");
    }

    TEST_F(TextEditorTests, BackspaceAndDelete)
    {
        Start("caf\xC3\xA9s"); // "cafés"
        EXPECT_TRUE(m_Editor.DeleteBackward(m_Text, false));
        EXPECT_EQ(m_Text, "caf\xC3\xA9");
        EXPECT_TRUE(m_Editor.DeleteBackward(m_Text, false)); // removes both bytes of "é"
        EXPECT_EQ(m_Text, "caf");

        m_Editor.MoveToStart(m_Text, false);
        EXPECT_FALSE(m_Editor.DeleteBackward(m_Text, false)); // nothing before the caret
        EXPECT_TRUE(m_Editor.DeleteForward(m_Text, false));
        EXPECT_EQ(m_Text, "af");
        m_Editor.MoveToEnd(m_Text, false);
        EXPECT_FALSE(m_Editor.DeleteForward(m_Text, false));

        Start("one two three");
        m_Editor.DeleteBackward(m_Text, true);
        EXPECT_EQ(m_Text, "one two ");
        m_Editor.MoveToStart(m_Text, false);
        m_Editor.DeleteForward(m_Text, true);
        EXPECT_EQ(m_Text, " two ");

        Start("selected text");
        m_Editor.SetCaret(m_Text, 0, false);
        m_Editor.SetCaret(m_Text, 9, true);
        m_Editor.DeleteBackward(m_Text, false); // with a selection, both keys delete it
        EXPECT_EQ(m_Text, "text");
        EXPECT_EQ(m_Editor.GetCaret(), 0u);
    }

    TEST_F(TextEditorTests, InsertSanitizesAndRespectsTheLimit)
    {
        Start("");
        m_Editor.Insert(m_Text, "two\nlines\tand\x01 control");
        EXPECT_EQ(m_Text, "two lines and control");

        Start("abc");
        EXPECT_TRUE(m_Editor.Insert(m_Text, "defgh", 5));
        EXPECT_EQ(m_Text, "abcde");
        EXPECT_FALSE(m_Editor.Insert(m_Text, "x", 5)); // full

        // The limit counts characters, not bytes, and replacing a selection frees room.
        Start("\xC3\xA4\xC3\xB6\xC3\xBC");                    // äöü
        EXPECT_TRUE(m_Editor.Insert(m_Text, "\xC3\x9F!", 4)); // room for one more character: ß
        EXPECT_EQ(m_Text, "\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x9F");
        m_Editor.SelectAll(m_Text);
        EXPECT_TRUE(m_Editor.Insert(m_Text, "12345678", 4));
        EXPECT_EQ(m_Text, "1234");
    }

    TEST_F(TextEditorTests, UndoAndRedo)
    {
        Start("Hello");
        // A run of typing is one step.
        m_Editor.Insert(m_Text, " ");
        m_Editor.Insert(m_Text, "w");
        m_Editor.Insert(m_Text, "o");
        EXPECT_EQ(m_Text, "Hello wo");
        // Deleting starts a new step; so does typing again afterwards.
        m_Editor.DeleteBackward(m_Text, false);
        m_Editor.DeleteBackward(m_Text, false);
        m_Editor.Insert(m_Text, "X");
        EXPECT_EQ(m_Text, "Hello X");

        EXPECT_TRUE(m_Editor.Undo(m_Text));
        EXPECT_EQ(m_Text, "Hello ");
        EXPECT_TRUE(m_Editor.Undo(m_Text));
        EXPECT_EQ(m_Text, "Hello wo");
        EXPECT_TRUE(m_Editor.Undo(m_Text));
        EXPECT_EQ(m_Text, "Hello");
        EXPECT_EQ(m_Editor.GetCaret(), 5u);
        EXPECT_FALSE(m_Editor.Undo(m_Text));

        EXPECT_TRUE(m_Editor.Redo(m_Text));
        EXPECT_EQ(m_Text, "Hello wo");
        EXPECT_TRUE(m_Editor.Redo(m_Text));
        EXPECT_TRUE(m_Editor.Redo(m_Text));
        EXPECT_EQ(m_Text, "Hello X");
        EXPECT_FALSE(m_Editor.Redo(m_Text));

        // A new edit discards what could be redone.
        m_Editor.Undo(m_Text);
        m_Editor.Insert(m_Text, "!");
        EXPECT_FALSE(m_Editor.Redo(m_Text));

        // Moving the caret ends a typing run.
        Start("");
        m_Editor.Insert(m_Text, "a");
        m_Editor.MoveLeft(m_Text, false, false);
        m_Editor.Insert(m_Text, "b");
        EXPECT_EQ(m_Text, "ba");
        m_Editor.Undo(m_Text);
        EXPECT_EQ(m_Text, "a");
    }

    TEST_F(TextEditorTests, ClampsWhenTheTextShrinksOutside)
    {
        Start("a long text");
        m_Text = "short";
        m_Editor.ClampTo(m_Text);
        EXPECT_EQ(m_Editor.GetCaret(), 5u);
        EXPECT_FALSE(m_Editor.Undo(m_Text)); // the history belonged to the old text
    }

    // ---- The widget, driven by simulated input ---------------------------------------------------------------

    class TextFieldTests : public WidgetTest
    {
    protected:
        void SetUp() override
        {
            WidgetTest::SetUp();
            // A host with a clipboard, like the examples provide through GLFW.
            DestroyContext(m_Context);
            ContextDescription description;
            description.Callbacks.AssertFailed = [this](const AssertInfo& info)
            { m_AssertMessages.emplace_back(info.Message); };
            description.Callbacks.GetClipboardText = [this] { return m_Clipboard; };
            description.Callbacks.SetClipboardText = [this](std::string_view text) { m_Clipboard = std::string(text); };
            description.Callbacks.SetCursor = [this](Cursor cursor) { m_Cursor = cursor; };
            m_Context = CreateContext(description);
            SetCurrentContext(m_Context);
            GetIO().SetDisplaySize(800.0f, 600.0f);
        }

        Builder Field()
        {
            return [this]
            {
                m_Changes += TextField("Name", &m_Text, m_Options) ? 1 : 0;
                m_Rect = GetItemRect();
                m_Submits += IsItemSubmitted() ? 1 : 0;
            };
        }

        // Clicks at the far right of the field: focus, caret at the end of the text.
        void FocusAtEnd()
        {
            Settle(Field());
            Click(Vec2(m_Rect.GetRight() - 10.0f, m_Rect.GetCenter().Y), Field());
        }

        const Internal::TextEditor& GetEditor() { return Internal::GetContext().TextEdit.Editor; }

        std::string m_Text;
        TextFieldOptions m_Options;
        Rect m_Rect;
        int m_Changes = 0;
        int m_Submits = 0;
        std::string m_Clipboard;
        Cursor m_Cursor = Cursor::Arrow;
    };

    TEST_F(TextFieldTests, HasAFixedDefaultSizeAndShowsAnIBeam)
    {
        Settle(Field());
        EXPECT_EQ(m_Rect.GetSize(), Vec2(180.0f, 24.0f));
        MoveMouse(m_Rect.GetCenter(), Field());
        EXPECT_EQ(m_Cursor, Cursor::IBeam);
        MoveMouse(Vec2(500.0f, 400.0f), Field());
        EXPECT_EQ(m_Cursor, Cursor::Arrow);
    }

    TEST_F(TextFieldTests, ClickFocusesAndTypingInserts)
    {
        Settle(Field());
        EXPECT_FALSE(GetIO().WantsTextInput());

        Click(m_Rect.GetCenter(), Field());
        EXPECT_TRUE(IsFocused(HashID("Name", HashID("Carbon"))));
        EXPECT_TRUE(GetIO().WantsTextInput());
        EXPECT_TRUE(GetIO().WantsKeyboard());

        Type("Carbon", Field());
        EXPECT_EQ(m_Text, "Carbon");
        EXPECT_GE(m_Changes, 1);
        Type(" \xC3\xBC\xE2\x82\xAC", Field());
        EXPECT_EQ(m_Text, "Carbon \xC3\xBC\xE2\x82\xAC");

        // Without focus, typing goes nowhere.
        Click(Vec2(500.0f, 400.0f), Field());
        EXPECT_FALSE(GetIO().WantsTextInput());
        Type("ignored", Field());
        EXPECT_EQ(m_Text, "Carbon \xC3\xBC\xE2\x82\xAC");
    }

    TEST_F(TextFieldTests, ClickPlacesTheCaretBetweenCharacters)
    {
        m_Text = "WWWWWWWWWW";
        Settle(Field());
        const TextSpec spec = GetTextSpec(TextStyle::Body);
        const float characterWidth = MeasureText("WWWWWWWWWW", spec).X / 10.0f;
        const float textLeft = m_Rect.X + 7.0f;

        // Just right of the boundary between the third and fourth character.
        Click(Vec2(textLeft + characterWidth * 3.0f + 1.0f, m_Rect.GetCenter().Y), Field());
        EXPECT_EQ(GetEditor().GetCaret(), 3u);
        Type("i", Field());
        EXPECT_EQ(m_Text, "WWWiWWWWWWW");

        // Left of the text: the very start.
        Click(Vec2(m_Rect.X + 2.0f, m_Rect.GetCenter().Y), Field());
        EXPECT_EQ(GetEditor().GetCaret(), 0u);
    }

    TEST_F(TextFieldTests, EditingKeys)
    {
        m_Text = "Hello world";
        FocusAtEnd();
        TapKey(Key::Backspace, Field());
        EXPECT_EQ(m_Text, "Hello worl");
        TapKey(Key::Home, Field());
        TapKey(Key::Delete, Field());
        EXPECT_EQ(m_Text, "ello worl");
        TapKey(Key::RightArrow, Field());
        Type("X", Field());
        EXPECT_EQ(m_Text, "eXllo worl");
        TapKey(Key::End, Field());
        Type("d!", Field());
        EXPECT_EQ(m_Text, "eXllo world!");

        // Ctrl+Backspace removes a word.
        TapKey(Key::LeftCtrl, Key::Backspace, Field());
        EXPECT_EQ(m_Text, "eXllo world");
        TapKey(Key::LeftCtrl, Key::Backspace, Field());
        EXPECT_EQ(m_Text, "eXllo ");
    }

    TEST_F(TextFieldTests, HeldKeysRepeat)
    {
        m_Text = "0123456789";
        FocusAtEnd();
        GetIO().AddKeyEvent(Key::Backspace, true);
        for (int i = 0; i < 45; i++)
            Frame(Field()); // 0.75 s: one press plus about seven repeats
        GetIO().AddKeyEvent(Key::Backspace, false);
        Frame(Field());
        EXPECT_LT(m_Text.size(), 5u);
        EXPECT_GT(m_Text.size(), 0u);
    }

    TEST_F(TextFieldTests, SelectionWithShiftAndSelectAll)
    {
        m_Text = "Hello world";
        FocusAtEnd();
        GetIO().AddKeyEvent(Key::LeftShift, true);
        Frame(Field());
        for (int i = 0; i < 5; i++)
            TapKey(Key::LeftArrow, Field());
        GetIO().AddKeyEvent(Key::LeftShift, false);
        Frame(Field());
        EXPECT_EQ(GetEditor().GetSelectedText(m_Text), "world");

        Type("there", Field());
        EXPECT_EQ(m_Text, "Hello there");

        TapKey(Key::LeftCtrl, Key::A, Field());
        EXPECT_EQ(GetEditor().GetSelectedText(m_Text), "Hello there");
        Type("x", Field());
        EXPECT_EQ(m_Text, "x");
    }

    TEST_F(TextFieldTests, DoubleClickSelectsAWordAndTripleClickEverything)
    {
        m_Text = "alpha beta gamma";
        Settle(Field());
        const TextSpec spec = GetTextSpec(TextStyle::Body);
        const float betaCenter = m_Rect.X + 7.0f + MeasureText("alpha be", spec).X;
        const Vec2 point(betaCenter, m_Rect.GetCenter().Y);

        MoveMouse(point, Field());
        PressMouse(Field());
        ReleaseMouse(Field());
        PressMouse(Field());
        ReleaseMouse(Field());
        EXPECT_EQ(GetEditor().GetSelectedText(m_Text), "beta");

        PressMouse(Field());
        ReleaseMouse(Field());
        EXPECT_EQ(GetEditor().GetSelectedText(m_Text), "alpha beta gamma");
    }

    TEST_F(TextFieldTests, DraggingSelects)
    {
        m_Text = "WWWWWWWWWW";
        Settle(Field());
        const float characterWidth = MeasureText(m_Text, GetTextSpec(TextStyle::Body)).X / 10.0f;
        const float textLeft = m_Rect.X + 7.0f;
        const float y = m_Rect.GetCenter().Y;

        MoveMouse(Vec2(textLeft + characterWidth * 2.0f, y), Field());
        PressMouse(Field());
        GetIO().AddMousePosEvent(textLeft + characterWidth * 6.0f, y);
        Frame(Field());
        EXPECT_EQ(GetEditor().GetSelectionStart(), 2u);
        EXPECT_EQ(GetEditor().GetSelectionEnd(), 6u);
        ReleaseMouse(Field());
        EXPECT_EQ(GetEditor().GetSelectedText(m_Text), "WWWW");
    }

    TEST_F(TextFieldTests, ClipboardThroughTheHostCallbacks)
    {
        m_Text = "copy me";
        FocusAtEnd();
        TapKey(Key::LeftCtrl, Key::A, Field());
        TapKey(Key::LeftCtrl, Key::C, Field());
        EXPECT_EQ(m_Clipboard, "copy me");
        EXPECT_EQ(m_Text, "copy me");

        TapKey(Key::LeftCtrl, Key::X, Field());
        EXPECT_EQ(m_Text, "");
        EXPECT_EQ(m_Clipboard, "copy me");

        m_Clipboard = "pasted\ntext";
        TapKey(Key::LeftCtrl, Key::V, Field());
        EXPECT_EQ(m_Text, "pasted text"); // a single-line field: the line break becomes a space
        TapKey(Key::LeftCtrl, Key::V, Field());
        EXPECT_EQ(m_Text, "pasted textpasted text");
    }

    TEST_F(TextFieldTests, UndoAndRedoShortcuts)
    {
        FocusAtEnd();
        Type("abc", Field());
        TapKey(Key::Backspace, Field());
        EXPECT_EQ(m_Text, "ab");
        TapKey(Key::LeftCtrl, Key::Z, Field());
        EXPECT_EQ(m_Text, "abc");
        TapKey(Key::LeftCtrl, Key::Z, Field());
        EXPECT_EQ(m_Text, "");
        TapKey(Key::LeftCtrl, Key::Y, Field());
        EXPECT_EQ(m_Text, "abc");

        // Shift+Ctrl+Z redoes as well.
        GetIO().AddKeyEvent(Key::LeftShift, true);
        TapKey(Key::LeftCtrl, Key::Z, Field());
        GetIO().AddKeyEvent(Key::LeftShift, false);
        Frame(Field());
        EXPECT_EQ(m_Text, "ab");
    }

    TEST_F(TextFieldTests, TabbingInSelectsEverything)
    {
        m_Text = "replace me";
        Settle(Field());
        TapKey(Key::Tab, Field());
        EXPECT_EQ(GetEditor().GetSelectedText(m_Text), "replace me");
        Type("new", Field());
        EXPECT_EQ(m_Text, "new");
    }

    TEST_F(TextFieldTests, EnterSubmitsAndEscapeGivesUpFocus)
    {
        m_Text = "query";
        FocusAtEnd();
        EXPECT_EQ(m_Submits, 0);
        TapKey(Key::Enter, Field());
        EXPECT_EQ(m_Submits, 1);
        EXPECT_EQ(m_Text, "query");
        EXPECT_TRUE(GetFocusedID().IsValid()); // submitting keeps the field focused

        TapKey(Key::Escape, Field());
        EXPECT_FALSE(GetFocusedID().IsValid());
        EXPECT_FALSE(GetIO().WantsTextInput());
    }

    TEST_F(TextFieldTests, MaxLengthLimitsTyping)
    {
        m_Options.MaxLength = 4;
        FocusAtEnd();
        Type("123456", Field());
        EXPECT_EQ(m_Text, "1234");
    }

    TEST_F(TextFieldTests, SecureFieldHidesAndProtectsItsText)
    {
        m_Options.IsSecure = true;
        FocusAtEnd();
        Type("s3cret", Field());
        EXPECT_EQ(m_Text, "s3cret");
        TapKey(Key::Backspace, Field());
        EXPECT_EQ(m_Text, "s3cre");

        // Nothing reaches the clipboard.
        TapKey(Key::LeftCtrl, Key::A, Field());
        TapKey(Key::LeftCtrl, Key::C, Field());
        TapKey(Key::LeftCtrl, Key::X, Field());
        EXPECT_EQ(m_Clipboard, "");
        EXPECT_EQ(m_Text, "s3cre");

        // On screen every character is the same glyph: a bullet.
        Click(Vec2(500.0f, 400.0f), Field());
        Settle(Field(), 3);
        const DrawData& drawData = GetDrawData();
        std::vector<Vec2> glyphSizes;
        for (size_t quad = 0; quad < drawData.Vertices.size() / 4; quad++)
        {
            if (GetPrimitive(quad * 4).Kind == DrawPrimitiveKind::Glyph)
                glyphSizes.push_back(drawData.Vertices[quad * 4 + 2].UV - drawData.Vertices[quad * 4].UV);
        }
        ASSERT_EQ(glyphSizes.size(), 5u);
        for (const Vec2& size : glyphSizes)
        {
            // The same height; the width may differ by a texel between sub-pixel positions.
            EXPECT_FLOAT_EQ(size.Y, glyphSizes[0].Y);
            EXPECT_NEAR(size.X, glyphSizes[0].X, 1.0f);
        }
    }

    TEST_F(TextFieldTests, PlaceholderShowsWhileEmpty)
    {
        const auto countGlyphs = [&]
        {
            size_t count = 0;
            for (size_t quad = 0; quad < GetDrawData().Vertices.size() / 4; quad++)
                count += GetPrimitive(quad * 4).Kind == DrawPrimitiveKind::Glyph ? 1 : 0;
            return count;
        };
        // The label doubles as the placeholder: "Name" is four glyphs.
        Settle(Field());
        EXPECT_EQ(countGlyphs(), 4u);

        m_Options.Placeholder = "Full name";
        Settle(Field(), 2);
        EXPECT_EQ(countGlyphs(), 8u); // the space has no glyph

        m_Text = "Ada";
        Settle(Field(), 2);
        EXPECT_EQ(countGlyphs(), 3u);
    }

    TEST_F(TextFieldTests, LongTextScrollsToKeepTheCaretVisible)
    {
        FocusAtEnd();
        Type("This sentence is much longer than a field of one hundred and eighty points", Field());
        Settle(Field(), 2);
        const Internal::TextEditState& edit = Internal::GetContext().TextEdit;
        EXPECT_GT(edit.ScrollX, 100.0f);
        const float caretX = edit.CaretPositions.back() - edit.ScrollX;
        EXPECT_LE(caretX, 180.0f - 14.0f + 0.01f); // inside the padded text area
        EXPECT_GE(caretX, 0.0f);

        TapKey(Key::Home, Field());
        EXPECT_FLOAT_EQ(Internal::GetContext().TextEdit.ScrollX, 0.0f);
    }

    TEST_F(TextFieldTests, ClearButtonEmptiesTheField)
    {
        m_Options.ShowsClearButton = true;
        m_Options.Icon = Icons::MagnifyingGlass;
        m_Text = "search term";
        Settle(Field());
        Click(Vec2(m_Rect.GetRight() - 14.0f, m_Rect.GetCenter().Y), Field());
        EXPECT_EQ(m_Text, "");
        EXPECT_GE(m_Changes, 1);
        EXPECT_TRUE(GetFocusedID().IsValid()); // ready for the next search
        Type("next", Field());
        EXPECT_EQ(m_Text, "next");
    }

    TEST_F(TextFieldTests, DisabledFieldCannotBeEdited)
    {
        m_Options.Disabled = true;
        m_Text = "locked";
        Settle(Field());
        Click(m_Rect.GetCenter(), Field());
        Type("x", Field());
        EXPECT_EQ(m_Text, "locked");
        EXPECT_FALSE(GetIO().WantsTextInput());
    }

    TEST_F(TextFieldTests, TextChangedByTheApplicationIsPickedUp)
    {
        m_Text = "a longer original";
        FocusAtEnd();
        m_Text = "new"; // the application replaces the text while the field is focused
        Frame(Field());
        Type("!", Field());
        EXPECT_EQ(m_Text, "new!");
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(TextFieldTests, ReportsTheSelectionWhileEditing)
    {
        m_Text = "Hello";
        TextFieldSelection selection;
        Settle(Field());
        EXPECT_FALSE(GetTextFieldSelection("Name", &selection)) << "not being edited";

        FocusAtEnd();
        ASSERT_TRUE(GetTextFieldSelection("Name", &selection));
        EXPECT_EQ(selection.Caret, 5u);
        EXPECT_EQ(selection.Start, selection.End);
        TapKey(Key::LeftShift, Key::LeftArrow, Field());
        TapKey(Key::LeftShift, Key::LeftArrow, Field());
        ASSERT_TRUE(GetTextFieldSelection("Name", &selection));
        EXPECT_EQ(selection.Caret, 3u);
        EXPECT_EQ(selection.Start, 3u);
        EXPECT_EQ(selection.End, 5u);
        TapKey(Key::Home, Field());
        ASSERT_TRUE(GetTextFieldSelection("Name", &selection));
        EXPECT_EQ(selection.Caret, 0u);
    }

    TEST_F(TextFieldTests, AFieldThatDoesNotAcceptInputKeepsFocusButIgnoresKeys)
    {
        m_Text = "Hello";
        FocusAtEnd();
        m_Options.AcceptsInput = false;
        Type("x", Field());
        TapKey(Key::Backspace, Field());
        TapKey(Key::LeftArrow, Field());
        TapKey(Key::Escape, Field());
        EXPECT_EQ(m_Text, "Hello");
        EXPECT_EQ(GetFocusedID(), GetID("Name")) << "Escape is left to the component around the field";
        EXPECT_FALSE(GetIO().WantsTextInput());
        TextFieldSelection selection;
        ASSERT_TRUE(GetTextFieldSelection("Name", &selection));
        EXPECT_EQ(selection.Caret, 5u);

        m_Options.AcceptsInput = true;
        Type("!", Field());
        EXPECT_EQ(m_Text, "Hello!");
    }

    TEST_F(TextFieldTests, AFieldWithoutBezelDrawsOnlyItsText)
    {
        m_Text = "Hi";
        m_Options.Width = 120.0f;
        Settle(Field());
        const size_t bezeled = GetDrawData().Vertices.size();
        m_Options.IsBezeled = false;
        Settle(Field());
        // Background and border are two quads.
        EXPECT_EQ(GetDrawData().Vertices.size(), bezeled - 8u);

        // Still a field: a click focuses it and typing edits the text.
        Click(Vec2(m_Rect.GetRight() - 10.0f, m_Rect.GetCenter().Y), Field());
        Type("!", Field());
        EXPECT_EQ(m_Text, "Hi!");
    }

    // ---- The other ways of passing the text: a fixed buffer and a callback ------------------------------------

    TEST_F(TextEditorTests, ByteLimitCutsAtCharacterBoundaries)
    {
        Start("ab");
        // Room for 3 more bytes: "c" and "ä" (2 bytes) fit, "€" (3 bytes) does not.
        EXPECT_TRUE(m_Editor.Insert(m_Text, "c\xC3\xA4\xE2\x82\xAC", 0, 5));
        EXPECT_EQ(m_Text, "abc\xC3\xA4");
        EXPECT_FALSE(m_Editor.Insert(m_Text, "x", 0, 5)) << "a full text rejects typing";
        EXPECT_EQ(m_Text, "abc\xC3\xA4");

        // Replacing a selection makes its bytes available again.
        m_Editor.SelectAll(m_Text);
        EXPECT_TRUE(m_Editor.Insert(m_Text, "12345678", 0, 5));
        EXPECT_EQ(m_Text, "12345");

        // When nothing fits in place of a selection, the selection stays.
        Start("\xE2\x82\xAC\xE2\x82\xAC"); // two 3-byte characters
        m_Editor.SetCaret(m_Text, 3, false);
        m_Editor.SetCaret(m_Text, 6, true);
        EXPECT_FALSE(m_Editor.Insert(m_Text, "\xF0\x9F\x98\x80", 0, 6)); // a 4-byte character into 3 bytes
        EXPECT_EQ(m_Text, "\xE2\x82\xAC\xE2\x82\xAC");
    }

    class TextFieldBufferTests : public TextFieldTests
    {
    protected:
        // A buffer of 8 bytes inside a larger array whose remaining bytes must never change.
        static constexpr size_t Capacity = 8;
        static constexpr char Canary = '#';

        void SetUp() override
        {
            TextFieldTests::SetUp();
            std::fill(std::begin(m_Storage), std::end(m_Storage), Canary);
            m_Storage[0] = '\0';
        }

        Builder BufferField()
        {
            return [this]
            {
                m_Changes += TextField("Name", std::span<char>(m_Storage, Capacity), m_Options) ? 1 : 0;
                m_Rect = GetItemRect();
            };
        }

        void FocusBufferAtEnd()
        {
            Settle(BufferField());
            Click(Vec2(m_Rect.GetRight() - 10.0f, m_Rect.GetCenter().Y), BufferField());
        }

        bool IsCanaryIntact() const
        {
            return std::all_of(std::begin(m_Storage) + Capacity, std::end(m_Storage),
                               [](char c) { return c == Canary; });
        }

        char m_Storage[Capacity + 8] = {};
    };

    TEST_F(TextFieldBufferTests, EditsTheBufferInPlaceAndKeepsItTerminated)
    {
        std::memcpy(m_Storage, "Ada", 4);
        FocusBufferAtEnd();
        Type("m", BufferField());
        EXPECT_STREQ(m_Storage, "Adam");
        EXPECT_EQ(m_Changes, 1);
        TapKey(Key::Backspace, BufferField());
        TapKey(Key::Backspace, BufferField());
        EXPECT_STREQ(m_Storage, "Ad");
        EXPECT_EQ(m_Changes, 3);
        EXPECT_TRUE(IsCanaryIntact());

        // A plain array converts to the span by itself.
        char name[16] = "Grace";
        bool changed = false;
        Frame([&] { changed = TextField("Plain", name); });
        EXPECT_FALSE(changed);
        EXPECT_STREQ(name, "Grace");
    }

    TEST_F(TextFieldBufferTests, TypingPastTheCapacityIsRejected)
    {
        FocusBufferAtEnd();
        Type("123456789012", BufferField());
        EXPECT_STREQ(m_Storage, "1234567") << "seven bytes of text and the terminating zero";
        EXPECT_TRUE(IsCanaryIntact());

        // A full buffer ignores more typing without reporting a change.
        const int changes = m_Changes;
        Type("x", BufferField());
        EXPECT_STREQ(m_Storage, "1234567");
        EXPECT_EQ(m_Changes, changes);

        // A multi-byte character that would not fit whole is not split.
        TapKey(Key::Backspace, BufferField());
        TapKey(Key::Backspace, BufferField());
        Type("\xE2\x82\xAC\xE2\x82\xAC", BufferField()); // two 3-byte characters into 2 free bytes
        EXPECT_STREQ(m_Storage, "12345");
        Type("\xC3\xA4", BufferField()); // a 2-byte character fits
        EXPECT_STREQ(m_Storage, "12345\xC3\xA4");
        EXPECT_TRUE(IsCanaryIntact());
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(TextFieldBufferTests, PastingAndUndoStayInsideTheBuffer)
    {
        std::memcpy(m_Storage, "ab", 3);
        FocusBufferAtEnd();
        m_Clipboard = "a long text from the clipboard";
        TapKey(Key::LeftCtrl, Key::V, BufferField());
        EXPECT_STREQ(m_Storage, "aba lon");
        TapKey(Key::LeftCtrl, Key::Z, BufferField());
        EXPECT_STREQ(m_Storage, "ab");
        TapKey(Key::LeftCtrl, Key::Y, BufferField());
        EXPECT_STREQ(m_Storage, "aba lon");
        EXPECT_TRUE(IsCanaryIntact());
    }

    TEST_F(TextFieldBufferTests, AnUnterminatedBufferIsReadUpToItsLastByteAndThenTerminated)
    {
        std::memset(m_Storage, 'x', Capacity); // no zero inside the buffer
        Settle(BufferField());
        EXPECT_EQ(std::string_view(m_Storage, Capacity), std::string_view("xxxxxxx\0", Capacity));
        EXPECT_TRUE(IsCanaryIntact());
    }

    TEST_F(TextFieldBufferTests, AnEmptyBufferIsReported)
    {
        Frame([this] { TextField("Name", std::span<char>()); });
        EXPECT_EQ(m_AssertMessages.size(), 1u);
    }

    TEST_F(TextFieldTests, CallbackFormReportsEachChangeOnce)
    {
        // Text the application stores in its own way: here, a vector of characters.
        std::vector<char> stored = {'H', 'i'};
        int calls = 0;
        const Builder field = [&]
        {
            const std::string_view current(stored.data(), stored.size());
            m_Changes += TextField(
                             "Name", current,
                             [&](std::string_view text)
                             {
                                 stored.assign(text.begin(), text.end());
                                 calls++;
                             },
                             m_Options)
                             ? 1
                             : 0;
            m_Rect = GetItemRect();
        };
        Settle(field);
        EXPECT_EQ(calls, 0) << "nothing is set while nothing changes";

        Click(Vec2(m_Rect.GetRight() - 10.0f, m_Rect.GetCenter().Y), field);
        Type("!", field);
        EXPECT_EQ(std::string_view(stored.data(), stored.size()), "Hi!");
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(m_Changes, 1);

        // Caret, selection and undo work as with a std::string.
        TapKey(Key::LeftCtrl, Key::A, field);
        Type("Bye", field);
        EXPECT_EQ(std::string_view(stored.data(), stored.size()), "Bye");
        // Replacing the selection and typing on are two steps, as with a std::string.
        TapKey(Key::LeftCtrl, Key::Z, field);
        TapKey(Key::LeftCtrl, Key::Z, field);
        EXPECT_EQ(std::string_view(stored.data(), stored.size()), "Hi!");
        EXPECT_EQ(calls, 4);

        // The text may change from outside between frames; the field shows what it is given.
        stored = {'N', 'e', 'w'};
        Frame(field);
        TapKey(Key::End, field);
        TapKey(Key::Backspace, field);
        EXPECT_EQ(std::string_view(stored.data(), stored.size()), "Ne");
    }

    TEST_F(TextFieldTests, CallbackFormAcceptsAFunction)
    {
        static std::string s_Stored;
        s_Stored = "x";
        struct Setter
        {
            static void Set(std::string_view text) { s_Stored = text; }
        };
        const Builder field = [&]
        {
            TextField("Name", s_Stored, &Setter::Set);
            m_Rect = GetItemRect();
        };
        Settle(field);
        Click(Vec2(m_Rect.GetRight() - 10.0f, m_Rect.GetCenter().Y), field);
        Type("y", field);
        EXPECT_EQ(s_Stored, "xy");
    }

    TEST_F(TextFieldTests, AllFormsShareOneImplementation)
    {
        // The same input gives the same text, caret and drawing whichever form holds the text.
        char buffer[64] = "Hello";
        std::string string = "Hello";
        std::string stored = "Hello";
        const auto drawsTheSame = [&](const Builder& field)
        {
            Settle(field);
            Click(Vec2(m_Rect.GetRight() - 10.0f, m_Rect.GetCenter().Y), field);
            Type(" there", field);
            TapKey(Key::LeftShift, Key::LeftArrow, field);
            TextFieldSelection selection;
            EXPECT_TRUE(GetTextFieldSelection("Name", &selection));
            EXPECT_EQ(selection.Start, 10u);
            EXPECT_EQ(selection.End, 11u);
            const size_t vertices = GetDrawData().Vertices.size();
            TapKey(Key::Escape, field);
            Settle(field);
            return vertices;
        };
        const size_t a = drawsTheSame(
            [&]
            {
                TextField("Name", &string);
                m_Rect = GetItemRect();
            });
        const size_t b = drawsTheSame(
            [&]
            {
                TextField("Name", buffer);
                m_Rect = GetItemRect();
            });
        const size_t c = drawsTheSame(
            [&]
            {
                TextField("Name", stored, [&](std::string_view text) { stored = text; });
                m_Rect = GetItemRect();
            });
        EXPECT_EQ(string, "Hello there");
        EXPECT_STREQ(buffer, "Hello there");
        EXPECT_EQ(stored, "Hello there");
        EXPECT_EQ(a, b);
        EXPECT_EQ(a, c);
    }
} // namespace Carbon
