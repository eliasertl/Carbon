#include <Carbon/Extensions/Extensions.h>

#include <algorithm>
#include <string>
#include <vector>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // What a host on a phone tells Carbon (safe area, text size, on-screen keyboard) and what Carbon does with it.
    class MobileHostTests : public WidgetTest
    {
    protected:
        void SetUp() override
        {
            WidgetTest::SetUp();
            GetIO().SetDisplaySize(390.0f, 844.0f);
            GetIO().SetTouchModeOverride(true);
        }
    };

    // ---- Safe area ------------------------------------------------------------------------------------------------

    TEST_F(MobileHostTests, ContentStaysInsideTheSafeArea)
    {
        GetIO().SetSafeAreaInsets(EdgeInsets(0.0f, 47.0f, 0.0f, 34.0f)); // an iPhone's status bar and home indicator
        Rect first;
        Rect bar;
        Settle(
            [&]
            {
                first = AllocateItem(Vec2(100.0f, 20.0f));
                BeginVStack({.Width = Size::Fill(),
                             .Height = Size::Fill(),
                             .Background = Color(1.0f, 0.0f, 0.0f),
                             .CornerRadius = 0.0f});
                EndVStack();
                bar = GetLastItemRect();
            });
        EXPECT_FLOAT_EQ(first.Y, 47.0f);
        EXPECT_FLOAT_EQ(bar.GetBottom(), 844.0f - 34.0f);

        // The square background still reaches the bottom edge, under the home indicator.
        float lowest = 0.0f;
        for (const DrawVertex& vertex : GetDrawData().Vertices)
            lowest = std::max(lowest, vertex.Position.Y);
        EXPECT_GE(lowest, 844.0f);
        Frame([] { EXPECT_EQ(GetSafeAreaInsets(), EdgeInsets(0.0f, 47.0f, 0.0f, 34.0f)); });
    }

    TEST_F(MobileHostTests, OverlaysAndSheetsStayInsideTheSafeArea)
    {
        GetIO().SetSafeAreaInsets(EdgeInsets(0.0f, 47.0f, 0.0f, 34.0f));
        Rect item;
        const Builder build = [&]
        {
            SetCursorPos(Vec2(20.0f, 300.0f));
            if (Button("Menu"))
                OpenOverlay(GetID("menu"));
            if (BeginOverlay(GetID("menu"), {.Anchor = GetItemRect(), .PresentsAsSheetInCompactWidth = true}))
            {
                item = AllocateItem(Vec2(100.0f, 30.0f));
                EndOverlay();
            }
        };
        Settle(build);
        Tap(Vec2(40.0f, 312.0f), build);
        Settle(build, 60);
        // The sheet stands on the bottom edge; its content keeps clear of the home indicator.
        EXPECT_LE(item.GetBottom(), 844.0f - 34.0f);
        EXPECT_GT(item.GetBottom(), 844.0f - 34.0f - 30.0f);
    }

    // ---- Text size ------------------------------------------------------------------------------------------------

    TEST_F(MobileHostTests, TextControlsAndRowsGrowWithTheTextScale)
    {
        GetIO().SetTouchModeOverride(false);
        TextSpec body;
        Rect button;
        Rect row;
        const Builder build = [&]
        {
            body = GetTextSpec(TextStyle::Body);
            Button("OK");
            button = GetItemRect();
            BeginList("list", {.Height = 200.0f});
            ListItem("Row", false);
            row = GetItemRect();
            EndList();
        };
        Settle(build);
        EXPECT_FLOAT_EQ(body.Size, 13.0f);
        EXPECT_FLOAT_EQ(button.Height, 24.0f);
        EXPECT_FLOAT_EQ(row.Height, 24.0f);

        GetIO().SetTextScale(1.5f);
        Settle(build);
        EXPECT_FLOAT_EQ(body.Size, 19.5f);
        EXPECT_FLOAT_EQ(body.LineHeight, 24.0f);
        EXPECT_FLOAT_EQ(button.Height, 36.0f);
        EXPECT_FLOAT_EQ(row.Height, 36.0f);
    }

    TEST_F(MobileHostTests, LargerTextWrapsOntoMoreLinesInsteadOfRunningOut)
    {
        Rect stack;
        Rect last;
        const Builder build = [&]
        {
            BeginHStack({.Spacing = 8.0f, .Width = 300.0f, .Wraps = true});
            for (const char* word : {"Carbon", "keeps", "its", "macOS", "look"})
            {
                Text(word);
                last = GetItemRect();
            }
            EndHStack();
            stack = GetLastItemRect();
        };
        Settle(build);
        const float oneLine = stack.Height;
        EXPECT_FLOAT_EQ(last.Y, 0.0f);
        GetIO().SetTextScale(3.0f);
        Settle(build);
        EXPECT_GT(stack.Height, oneLine * 4.0f); // several lines of three times the height
        EXPECT_LE(stack.Width, 300.0f);
    }

    // ---- On-screen keyboard ---------------------------------------------------------------------------------------

    class KeyboardTests : public MobileHostTests
    {
    protected:
        void SetUp() override
        {
            MobileHostTests::SetUp();
            ContextDescription description;
            description.Callbacks.SetKeyboardVisible = [this](bool visible) { m_KeyboardCalls.push_back(visible); };
            DestroyContext(m_Context);
            m_Context = CreateContext(description);
            SetCurrentContext(m_Context);
            GetIO().SetDisplaySize(390.0f, 844.0f);
            GetIO().SetTouchModeOverride(true);
        }

        Builder Form()
        {
            return [this]
            {
                SetCursorPos(Vec2(20.0f, 100.0f));
                TextField("Name", &m_Name, {.Width = 300.0f});
                m_NameRect = GetItemRect();
                SetCursorPos(Vec2(20.0f, 200.0f));
                TextField("Amount", &m_Amount, {.Width = 300.0f, .Keyboard = KeyboardType::Number});
                m_AmountRect = GetItemRect();
            };
        }

        std::vector<bool> m_KeyboardCalls;
        std::string m_Name = "Helo world";
        std::string m_Amount = "12";
        Rect m_NameRect;
        Rect m_AmountRect;
    };

    TEST_F(KeyboardTests, TappingAFieldAsksForTheKeyboardAndTappingOutsideHidesIt)
    {
        Settle(Form());
        // Before any tap the host knows where a tap would start editing.
        const std::span<const Rect> areas = GetIO().GetTextInputAreas();
        ASSERT_EQ(areas.size(), 2u);
        EXPECT_TRUE(areas[0].Contains(m_NameRect.GetCenter()));
        EXPECT_TRUE(m_KeyboardCalls.empty());

        Tap(m_NameRect.GetCenter(), Form());
        ASSERT_EQ(m_KeyboardCalls.size(), 1u);
        EXPECT_TRUE(m_KeyboardCalls[0]);
        const TextInputState& state = GetIO().GetTextInputState();
        EXPECT_EQ(state.Text, "Helo world");
        EXPECT_LE(state.SelectionStart, state.Text.size());
        EXPECT_EQ(state.Keyboard, KeyboardType::Text);
        EXPECT_FALSE(state.IsMultiLine);

        // Another field takes over: the keyboard is asked for again, a numeric one this time.
        Tap(m_AmountRect.GetCenter(), Form());
        ASSERT_EQ(m_KeyboardCalls.size(), 2u);
        EXPECT_TRUE(m_KeyboardCalls[1]);
        EXPECT_EQ(GetIO().GetTextInputState().Keyboard, KeyboardType::Number);

        Tap(Vec2(200.0f, 600.0f), Form());
        Frame(Form());
        ASSERT_EQ(m_KeyboardCalls.size(), 3u);
        EXPECT_FALSE(m_KeyboardCalls[2]);
        EXPECT_FALSE(GetIO().WantsTextInput());
    }

    TEST_F(KeyboardTests, AKeyboardReplacesTextForAutocorrection)
    {
        Settle(Form());
        Tap(m_NameRect.GetCenter(), Form());
        // "Helo" becomes "Hello", as an autocorrection would make it.
        GetIO().AddTextReplaceEvent(0, 4, "Hello");
        Frame(Form());
        EXPECT_EQ(m_Name, "Hello world");
        EXPECT_EQ(GetIO().GetTextInputState().SelectionStart, 5u);
        EXPECT_EQ(GetIO().GetTextInputState().SelectionEnd, 5u);

        // A deletion is a replacement with nothing; characters typed afterwards follow it in order.
        GetIO().AddTextReplaceEvent(5, 11, "");
        GetIO().AddInputCharactersUTF8("!");
        Frame(Form());
        Frame(Form());
        EXPECT_EQ(m_Name, "Hello!");
    }

    TEST_F(KeyboardTests, TheFieldIsScrolledAboveTheKeyboard)
    {
        Rect field;
        std::string text = "Bottom";
        const Builder build = [&]
        {
            BeginScrollView("form", {.Spacing = 0.0f});
            AllocateItem(Vec2(100.0f, 700.0f));
            TextField("Bottom", &text, {.Width = 300.0f});
            field = GetItemRect();
            AllocateItem(Vec2(100.0f, 400.0f));
            EndScrollView();
        };
        Settle(build);
        Tap(field.GetCenter(), build);
        GetIO().SetKeyboardRect(Rect(0.0f, 500.0f, 390.0f, 344.0f));
        Settle(build, 60);
        EXPECT_LE(field.GetBottom(), 500.0f);
        EXPECT_GT(field.GetBottom(), 400.0f); // just above it, not far
    }

    TEST_F(KeyboardTests, WithoutAScrollViewTheInterfaceMovesUp)
    {
        std::string text;
        Rect field;
        const Builder build = [&]
        {
            AllocateItem(Vec2(100.0f, 760.0f));
            TextField("Low", &text, {.Width = 300.0f});
            field = GetItemRect();
        };
        Settle(build);
        const float resting = field.Y;
        Tap(field.GetCenter(), build);
        GetIO().SetKeyboardRect(Rect(0.0f, 500.0f, 390.0f, 344.0f));
        Settle(build, 60);
        EXPECT_LE(field.GetBottom(), 500.0f);

        // When the keyboard goes, the interface returns.
        GetIO().SetKeyboardRect(Rect());
        Settle(build, 60);
        EXPECT_NEAR(field.Y, resting, 0.5f);
    }

    TEST_F(KeyboardTests, SheetsStandOnTheKeyboard)
    {
        GetIO().SetKeyboardRect(Rect(0.0f, 500.0f, 390.0f, 344.0f));
        Rect item;
        const Builder build = [&]
        {
            if (BeginOverlay(GetID("sheet"), {.DismissOnOutsideClick = false, .PresentsAsSheetInCompactWidth = true}))
            {
                item = AllocateItem(Vec2(100.0f, 30.0f));
                EndOverlay();
            }
        };
        OpenOverlay(GetID("sheet"));
        Settle(build, 60);
        EXPECT_LE(item.GetBottom(), 500.0f);
        EXPECT_GT(item.GetBottom(), 450.0f);
    }

    // ---- Rotation -------------------------------------------------------------------------------------------------

    TEST_F(MobileHostTests, RotatingReflowsWithoutLosingState)
    {
        ShowNavigationDetail("main", true, false);
        Rect content;
        const Builder build = [&]
        {
            BeginNavigationSplitView("main");
            BeginSidebar("pages");
            SidebarItem("Inbox", true);
            EndSidebar();
            NavigationSplitViewDetail();
            content = AllocateItem(Vec2(100.0f, 40.0f), {.Width = Size::Fill()});
            EndNavigationSplitView();
        };
        Settle(build);
        EXPECT_FLOAT_EQ(content.X, 0.0f); // compact: the detail fills the display
        EXPECT_FLOAT_EQ(content.Width, 390.0f);

        GetIO().SetDisplaySize(844.0f, 390.0f); // landscape: regular width
        Settle(build);
        EXPECT_FLOAT_EQ(content.X, 220.0f);
        EXPECT_FLOAT_EQ(content.Width, 844.0f - 220.0f);

        GetIO().SetDisplaySize(390.0f, 844.0f); // and back, still showing the detail
        Settle(build);
        EXPECT_FLOAT_EQ(content.X, 0.0f);
    }
} // namespace Carbon
