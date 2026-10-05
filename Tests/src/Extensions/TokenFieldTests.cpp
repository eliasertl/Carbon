#include <Carbon/Extensions/Extensions.h>

#include <string>
#include <vector>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // ---- TokenField ---------------------------------------------------------------------------------------------

    class TokenFieldTests : public WidgetTest
    {
    protected:
        using Tokens = std::vector<std::string>;

        // The field, then a button to move the focus to.
        Builder Interface()
        {
            return [this]
            {
                BeginVStack({.Spacing = 10.0f});
                if (TokenField("tokens", &m_Tokens, m_Options))
                    m_Changes++;
                m_Rect = GetItemRect();
                m_Id = GetItemID();
                m_Submits += IsItemSubmitted() ? 1 : 0;
                m_MenuToken = -1;
                int token = -1;
                if (BeginTokenFieldMenu("tokens", &token))
                {
                    m_MenuToken = token;
                    MenuItem("Remove");
                    EndTokenFieldMenu();
                }
                Button("Next");
                EndVStack();
            };
        }

        // The centre of a token on the first line: tokens are 6 points of padding around their text, 4 apart,
        // starting 3 points inside the field.
        Vec2 GetToken(size_t index) const
        {
            const TextSpec spec = GetTextSpec(TextStyle::Body);
            float x = m_Rect.X + 3.0f;
            for (size_t i = 0; i < index; i++)
                x += MeasureText(m_Tokens[i], spec).X + 12.0f + 4.0f;
            return Vec2(x + (MeasureText(m_Tokens[index], spec).X + 12.0f) * 0.5f, m_Rect.Y + 12.0f);
        }

        void Focus()
        {
            Settle(Interface());
            Click(Vec2(m_Rect.GetRight() - 8.0f, m_Rect.GetBottom() - 8.0f), Interface());
            ASSERT_EQ(GetFocusedID(), m_Id);
        }

        Tokens m_Tokens;
        TokenFieldOptions m_Options;
        int m_Changes = 0;
        int m_Submits = 0;
        int m_MenuToken = -1;
        Rect m_Rect;
        ID m_Id;
    };

    TEST_F(TokenFieldTests, ACommaOrReturnTurnsTextIntoAToken)
    {
        Focus();
        Type("Ada, Grace", Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"Ada"}));
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"Ada", "Grace"}));
        EXPECT_EQ(m_Changes, 2);
        EXPECT_EQ(m_Submits, 0) << "Return made a token";

        // With nothing typed, Return submits.
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Submits, 1);
        EXPECT_EQ(m_Tokens.size(), 2u);

        // Several at once, as from a paste; empty parts and spaces disappear.
        Type("a, ,b,c ,", Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"Ada", "Grace", "a", "b", "c"}));
    }

    TEST_F(TokenFieldTests, OtherDelimitersAndNoReturn)
    {
        m_Options.Delimiters = ";";
        m_Options.TokenizesOnReturn = false;
        Focus();
        Type("x,y;z", Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"x,y"}));
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Tokens.size(), 1u);
        EXPECT_EQ(m_Submits, 1);
    }

    TEST_F(TokenFieldTests, LosingFocusTurnsTheTextIntoAToken)
    {
        Focus();
        Type("Bob", Interface());
        EXPECT_TRUE(m_Tokens.empty());
        TapKey(Key::Tab, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"Bob"}));
    }

    TEST_F(TokenFieldTests, BackspaceSelectsTheTokenBeforeTheTextThenDeletesIt)
    {
        m_Tokens = {"A", "B"};
        Focus();
        TapKey(Key::Backspace, Interface());
        EXPECT_EQ(m_Tokens.size(), 2u) << "the first Backspace only selects";
        TapKey(Key::Backspace, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"A"}));
        TapKey(Key::Backspace, Interface());
        TapKey(Key::Delete, Interface());
        EXPECT_TRUE(m_Tokens.empty());
        EXPECT_EQ(m_Changes, 2);

        // With text before the caret, Backspace edits the text.
        m_Tokens = {"A"};
        Type("xy", Interface());
        TapKey(Key::Backspace, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"A"}));
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"A", "x"}));
    }

    TEST_F(TokenFieldTests, ArrowsMoveBetweenTokensAndShiftExtends)
    {
        m_Tokens = {"A", "B", "C"};
        Focus();
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::LeftShift, Key::LeftArrow, Interface());
        TapKey(Key::Delete, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"C"}));

        // Right past the last token returns to the text.
        m_Tokens = {"A", "B", "C"};
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::RightArrow, Interface());
        Type("x", Interface());
        EXPECT_EQ(m_Tokens.size(), 3u);
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"A", "B", "C", "x"}));
    }

    TEST_F(TokenFieldTests, TypingReplacesSelectedTokens)
    {
        m_Tokens = {"A", "B", "C"};
        Focus();
        TapKey(Key::LeftArrow, Interface());
        Type("D,", Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"A", "B", "D"}));

        // Select all, then type.
        TapKey(Key::LeftCtrl, Key::A, Interface());
        Type("E", Interface());
        EXPECT_TRUE(m_Tokens.empty());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"E"}));
    }

    TEST_F(TokenFieldTests, ClickingSelectsTokensAndEscapeDeselects)
    {
        m_Tokens = {"Ada", "Grace", "Alan"};
        Settle(Interface());
        Click(GetToken(0), Interface());
        EXPECT_EQ(GetFocusedID(), m_Id);
        GetIO().AddKeyEvent(Key::LeftShift, true);
        Click(GetToken(1), Interface());
        GetIO().AddKeyEvent(Key::LeftShift, false);
        TapKey(Key::Delete, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"Alan"}));

        Click(GetToken(0), Interface());
        TapKey(Key::Escape, Interface());
        TapKey(Key::Delete, Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"Alan"})) << "nothing selected after Escape";
    }

    TEST_F(TokenFieldTests, WrappingGrowsTheFieldAndMaxLinesCapsIt)
    {
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Rect.Height, 24.0f);
        m_Tokens = {"Ada Lovelace", "Grace Hopper", "Alan Kay", "Edsger Dijkstra", "Barbara Liskov", "Ken Thompson"};
        Settle(Interface(), 60);
        EXPECT_GT(m_Rect.Height, 24.0f + 18.0f);
        const float grown = m_Rect.Height;

        m_Options.MaxLines = 2;
        Settle(Interface(), 60);
        EXPECT_FLOAT_EQ(m_Rect.Height, 3.0f * 2.0f + 18.0f * 2.0f + 3.0f);
        EXPECT_LT(m_Rect.Height, grown);

        m_Options.MaxLines = 0;
        m_Options.Layout = TokenFieldLayout::SingleLine;
        Settle(Interface(), 60);
        EXPECT_FLOAT_EQ(m_Rect.Height, 24.0f) << "a single line scrolls instead of growing";
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(TokenFieldTests, ASingleLineScrollsToTheTextWhileEditing)
    {
        m_Options.Layout = TokenFieldLayout::SingleLine;
        m_Tokens = {"Ada Lovelace", "Grace Hopper", "Alan Kay", "Edsger Dijkstra", "Barbara Liskov"};
        Settle(Interface());
        // Unfocused, the first token is at the start.
        const Vec2 first = GetToken(0);
        TapKey(Key::Tab, Interface());
        ASSERT_EQ(GetFocusedID(), m_Id);
        Settle(Interface(), 60);
        // Focused, the field has scrolled towards the text: another token lies where the first one was.
        Click(first, Interface());
        TapKey(Key::Delete, Interface());
        EXPECT_EQ(m_Tokens.size(), 4u);
        EXPECT_EQ(m_Tokens.front(), "Ada Lovelace");
    }

    TEST_F(TokenFieldTests, ARightClickOpensTheTokensMenu)
    {
        m_Tokens = {"Ada", "Grace"};
        Settle(Interface());
        MoveMouse(GetToken(1), Interface());
        PressMouse(Interface(), MouseButton::Right);
        ReleaseMouse(Interface(), MouseButton::Right);
        Settle(Interface(), 3);
        EXPECT_EQ(m_MenuToken, 1);
    }

    TEST_F(TokenFieldTests, DisabledIgnoresInput)
    {
        m_Tokens = {"A"};
        m_Options.Disabled = true;
        Settle(Interface());
        Click(GetToken(0), Interface());
        Click(Vec2(m_Rect.GetRight() - 8.0f, m_Rect.GetCenter().Y), Interface());
        EXPECT_NE(GetFocusedID(), m_Id);
        Type("x,", Interface());
        EXPECT_EQ(m_Tokens, (Tokens{"A"}));
        EXPECT_TRUE(m_AssertMessages.empty());
    }
} // namespace Carbon
