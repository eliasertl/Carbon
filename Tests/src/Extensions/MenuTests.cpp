#include <Carbon/Extensions/Extensions.h>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // ---- Menu ---------------------------------------------------------------------------------------------------

    class MenuTests : public WidgetTest
    {
    protected:
        // A menu below a 60 x 24 anchor at (100, 100): First, a disabled item, a separator, a submenu with one
        // item, Last.
        void Build()
        {
            MenuOptions options;
            options.Anchor = Rect(100.0f, 100.0f, 60.0f, 24.0f);
            m_IsSubmenuShown = false;
            if (BeginMenu("menu", options))
            {
                if (MenuItem("First"))
                    m_Chosen = 1;
                m_FirstRect = GetItemRect();
                m_FirstID = GetItemID();
                if (MenuItem("Disabled", {.Disabled = true}))
                    m_Chosen = 2;
                MenuSeparator();
                if (BeginSubmenu("More"))
                {
                    m_IsSubmenuShown = true;
                    if (MenuItem("Deep"))
                        m_Chosen = 3;
                    m_DeepRect = GetItemRect();
                    m_DeepID = GetItemID();
                    EndSubmenu();
                }
                if (MenuItem("Last", {.Shortcut = "Ctrl+L"}))
                    m_Chosen = 4;
                m_LastID = GetItemID();
                EndMenu();
            }
        }

        Builder Interface()
        {
            return [this] { Build(); };
        }

        // Rows are 22 points high and the separator 11.
        Vec2 GetDisabledCenter() const { return m_FirstRect.GetCenter() + Vec2(0.0f, 22.0f); }
        Vec2 GetMoreCenter() const { return m_FirstRect.GetCenter() + Vec2(0.0f, 55.0f); }

        void OpenAndSettle()
        {
            OpenMenu("menu");
            Settle(Interface());
        }

        int m_Chosen = 0;
        bool m_IsSubmenuShown = false;
        Rect m_FirstRect;
        Rect m_DeepRect;
        ID m_FirstID;
        ID m_DeepID;
        ID m_LastID;
    };

    TEST_F(MenuTests, OpensBelowItsAnchor)
    {
        Frame(Interface());
        EXPECT_FALSE(IsMenuOpen("menu"));

        OpenAndSettle();
        EXPECT_TRUE(IsMenuOpen("menu"));
        // Gap of 2 below the anchor, then 5 points of padding around the rows.
        EXPECT_FLOAT_EQ(m_FirstRect.X, 105.0f);
        EXPECT_FLOAT_EQ(m_FirstRect.Y, 131.0f);
        EXPECT_FLOAT_EQ(m_FirstRect.Height, 22.0f);
    }

    TEST_F(MenuTests, ChoosingAnItemClosesTheMenu)
    {
        OpenAndSettle();
        Click(m_FirstRect.GetCenter(), Interface());
        EXPECT_EQ(m_Chosen, 1);
        EXPECT_FALSE(IsMenuOpen("menu"));
    }

    TEST_F(MenuTests, DisabledItemsCannotBeChosen)
    {
        OpenAndSettle();
        Click(GetDisabledCenter(), Interface());
        EXPECT_EQ(m_Chosen, 0);
        EXPECT_TRUE(IsMenuOpen("menu"));
    }

    TEST_F(MenuTests, HighlightFollowsThePointer)
    {
        OpenAndSettle();
        EXPECT_FALSE(GetFocusedID().IsValid());
        MoveMouse(m_FirstRect.GetCenter(), Interface());
        EXPECT_EQ(GetFocusedID(), m_FirstID);
    }

    TEST_F(MenuTests, ArrowKeysMoveTheHighlightAndEnterChooses)
    {
        OpenAndSettle();
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(GetFocusedID(), m_FirstID);
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(GetFocusedID(), m_LastID) << "the highlight wraps around and skips nothing but disabled items";

        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Chosen, 4);
        EXPECT_FALSE(IsMenuOpen("menu"));
    }

    TEST_F(MenuTests, EscapeAndOutsideClicksClose)
    {
        OpenAndSettle();
        TapKey(Key::Escape, Interface());
        EXPECT_FALSE(IsMenuOpen("menu"));

        OpenAndSettle();
        Click(Vec2(600.0f, 500.0f), Interface());
        EXPECT_FALSE(IsMenuOpen("menu"));
        EXPECT_EQ(m_Chosen, 0);
    }

    TEST_F(MenuTests, RightArrowOpensASubmenuAndLeftArrowClosesIt)
    {
        OpenAndSettle();
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_FALSE(m_IsSubmenuShown);

        TapKey(Key::RightArrow, Interface());
        EXPECT_TRUE(m_IsSubmenuShown);
        EXPECT_EQ(GetFocusedID(), m_DeepID) << "a submenu opened by keyboard highlights its first item";

        TapKey(Key::LeftArrow, Interface());
        EXPECT_FALSE(m_IsSubmenuShown);
        EXPECT_TRUE(IsMenuOpen("menu"));
    }

    TEST_F(MenuTests, ChoosingInASubmenuClosesTheWholeChain)
    {
        OpenAndSettle();
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::RightArrow, Interface());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Chosen, 3);
        EXPECT_FALSE(IsMenuOpen("menu"));
        Frame(Interface());
        EXPECT_FALSE(m_IsSubmenuShown);
    }

    TEST_F(MenuTests, RestingOnASubmenuItemOpensIt)
    {
        OpenAndSettle();
        MoveMouse(GetMoreCenter(), Interface());
        EXPECT_FALSE(m_IsSubmenuShown);
        for (int i = 0; i < 4; i++)
            Frame(Interface(), 0.1f);
        EXPECT_TRUE(m_IsSubmenuShown);

        // The submenu sits at the trailing edge of the menu, its first row level with its item.
        Settle(Interface());
        EXPECT_GT(m_DeepRect.X, m_FirstRect.GetRight());
        EXPECT_FLOAT_EQ(m_DeepRect.Y, m_FirstRect.Y + 55.0f);

        Click(m_DeepRect.GetCenter(), Interface());
        EXPECT_EQ(m_Chosen, 3);
        EXPECT_FALSE(IsMenuOpen("menu"));
    }

    TEST_F(MenuTests, ClickOutsideClosesMenuAndSubmenu)
    {
        OpenAndSettle();
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::RightArrow, Interface());
        Settle(Interface());
        ASSERT_TRUE(m_IsSubmenuShown);

        Click(Vec2(600.0f, 500.0f), Interface());
        Frame(Interface());
        EXPECT_FALSE(m_IsSubmenuShown);
        EXPECT_FALSE(IsMenuOpen("menu"));
    }

    TEST_F(MenuTests, ReturningToTheParentMenuClosesTheSubmenu)
    {
        OpenAndSettle();
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::RightArrow, Interface());
        Settle(Interface());
        ASSERT_TRUE(m_IsSubmenuShown);

        MoveMouse(m_FirstRect.GetCenter(), Interface());
        for (int i = 0; i < 4; i++)
            Frame(Interface(), 0.1f);
        EXPECT_FALSE(m_IsSubmenuShown);
        EXPECT_TRUE(IsMenuOpen("menu"));
    }

    TEST_F(MenuTests, ItemsOutsideAMenuAreReported)
    {
        Frame([] { MenuItem("Stray"); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("BeginMenu"), std::string::npos);
    }

    TEST_F(MenuTests, ContextMenuOpensAtThePointerOnRightClick)
    {
        Rect itemRect;
        const Builder build = [&]
        {
            const Rect area = AllocateItem(Vec2(300.0f, 200.0f));
            ButtonBehavior(GetID("area"), area);
            if (BeginContextMenu("context"))
            {
                if (MenuItem("Rename"))
                    m_Chosen = 7;
                itemRect = GetItemRect();
                EndContextMenu();
            }
        };
        Settle(build);
        MoveMouse(Vec2(120.0f, 80.0f), build);
        PressMouse(build, MouseButton::Left);
        ReleaseMouse(build, MouseButton::Left);
        EXPECT_FALSE(IsMenuOpen("context"));

        PressMouse(build, MouseButton::Right);
        ReleaseMouse(build, MouseButton::Right);
        EXPECT_TRUE(IsMenuOpen("context"));
        Settle(build);
        EXPECT_FLOAT_EQ(itemRect.X, 125.0f);
        EXPECT_FLOAT_EQ(itemRect.Y, 85.0f);

        Click(itemRect.GetCenter(), build);
        EXPECT_EQ(m_Chosen, 7);
        EXPECT_FALSE(IsMenuOpen("context"));
    }

    // ---- PopUpButton and PullDownButton -------------------------------------------------------------------------

    class PopUpButtonTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                BeginVStack({.Padding = EdgeInsets(200.0f, 200.0f)});
                if (PopUpButton("popup", &m_Selected, {"Small", "Medium", "Large"}))
                    m_Changes++;
                m_Rect = GetItemRect();
                m_Id = GetItemID();
                EndVStack();
            };
        }

        int m_Selected = 1;
        int m_Changes = 0;
        Rect m_Rect;
        ID m_Id;
    };

    TEST_F(PopUpButtonTests, MenuOpensOverTheButtonAndChoosingChangesTheSelection)
    {
        Settle(Interface());
        EXPECT_FALSE(IsAnyOverlayOpen());
        Click(m_Rect.GetCenter(), Interface());
        EXPECT_TRUE(IsAnyOverlayOpen());
        Settle(Interface());

        // The current item lies over the button; the next one is a row further down.
        Click(m_Rect.GetCenter() + Vec2(0.0f, 22.0f), Interface());
        EXPECT_EQ(m_Selected, 2);
        EXPECT_EQ(m_Changes, 1);
        EXPECT_FALSE(IsAnyOverlayOpen());
    }

    TEST_F(PopUpButtonTests, ChoosingTheCurrentItemChangesNothing)
    {
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        EXPECT_EQ(m_Selected, 1);
        EXPECT_EQ(m_Changes, 0);
        EXPECT_FALSE(IsAnyOverlayOpen());
    }

    TEST_F(PopUpButtonTests, KeyboardOpensWithTheCurrentItemHighlighted)
    {
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), m_Id);
        TapKey(Key::Space, Interface());
        EXPECT_TRUE(IsAnyOverlayOpen());

        TapKey(Key::UpArrow, Interface());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Selected, 0);
        EXPECT_FALSE(IsAnyOverlayOpen());
        // Focus is back on the button.
        EXPECT_EQ(GetFocusedID(), m_Id);
    }

    TEST_F(PopUpButtonTests, PullDownButtonRunsCommands)
    {
        int chosen = 0;
        Rect button;
        Rect item;
        const Builder build = [&]
        {
            const bool isOpen = BeginPullDownButton("Add");
            button = GetItemRect();
            if (isOpen)
            {
                if (MenuItem("Folder"))
                    chosen = 1;
                if (MenuItem("Document"))
                    chosen = 2;
                item = GetItemRect();
                EndPullDownButton();
            }
        };
        Settle(build);
        Click(button.GetCenter(), build);
        EXPECT_TRUE(IsAnyOverlayOpen());
        Settle(build);
        EXPECT_GT(item.Y, button.GetBottom());
        EXPECT_GE(item.Width, button.Width - 10.0f);

        Click(item.GetCenter(), build);
        EXPECT_EQ(chosen, 2);
        EXPECT_FALSE(IsAnyOverlayOpen());
    }

    // ---- Popover ------------------------------------------------------------------------------------------------

    class PopoverTests : public WidgetTest
    {
    protected:
        Rect m_Button;
    };

    TEST_F(PopoverTests, AttachesToTheLastItem)
    {
        Rect content;
        bool isShown = false;
        const Builder build = [&]
        {
            BeginVStack({.Padding = EdgeInsets(300.0f, 100.0f)});
            if (Button("Info"))
                OpenPopover("info");
            const Rect button = GetItemRect();
            isShown = false;
            if (BeginPopover("info"))
            {
                isShown = true;
                content = AllocateItem(Vec2(100.0f, 40.0f));
                EndPopover();
            }
            EndVStack();
            m_Button = button;
        };
        Settle(build);
        EXPECT_FALSE(isShown);
        EXPECT_FALSE(IsPopoverOpen("info"));

        Click(m_Button.GetCenter(), build);
        Settle(build);
        EXPECT_TRUE(isShown);
        // Centered below the button: gap 2, arrow 7, padding 16.
        EXPECT_FLOAT_EQ(content.Y, m_Button.GetBottom() + 9.0f + 16.0f);
        EXPECT_NEAR(content.GetCenter().X, m_Button.GetCenter().X, 0.5f);

        Click(Vec2(700.0f, 500.0f), build);
        EXPECT_FALSE(isShown);
    }

    TEST_F(PopoverTests, CanBeClosedFromCode)
    {
        const Builder build = []
        {
            if (BeginPopover("info", {.Anchor = Rect(100.0f, 100.0f, 50.0f, 20.0f)}))
                EndPopover();
        };
        OpenPopover("info");
        Settle(build);
        EXPECT_TRUE(IsPopoverOpen("info"));
        ClosePopover("info");
        EXPECT_FALSE(IsPopoverOpen("info"));
    }

    // ---- Alert and Sheet ----------------------------------------------------------------------------------------

    class AlertTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                m_Under = ButtonBehavior(HashID("under"), Rect(0.0f, 0.0f, 800.0f, 600.0f));
                m_UnderClicks += m_Under.Clicked ? 1 : 0;
                const AlertResult result = Alert("alert", "Delete the file?", m_Options);
                if (result != AlertResult::None)
                {
                    m_Result = result;
                    m_Answers++;
                }
            };
        }

        void OpenAndSettle()
        {
            OpenAlert("alert");
            Settle(Interface());
        }

        AlertOptions m_Options = {
            .Message = "You cannot undo this.", .PrimaryLabel = "Delete", .SecondaryLabel = "Cancel"};
        AlertResult m_Result = AlertResult::None;
        int m_Answers = 0;
        Interaction m_Under;
        int m_UnderClicks = 0;
    };

    TEST_F(AlertTests, ReturnsNothingWhileClosed)
    {
        Settle(Interface());
        EXPECT_EQ(m_Answers, 0);
        EXPECT_FALSE(IsAnyOverlayOpen());
    }

    TEST_F(AlertTests, EnterChoosesThePrimaryButton)
    {
        OpenAndSettle();
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Result, AlertResult::Primary);
        EXPECT_EQ(m_Answers, 1);
        EXPECT_FALSE(IsAnyOverlayOpen());
    }

    TEST_F(AlertTests, EscapeChoosesTheSecondaryButton)
    {
        OpenAndSettle();
        TapKey(Key::Escape, Interface());
        EXPECT_EQ(m_Result, AlertResult::Secondary);
        EXPECT_EQ(m_Answers, 1);
        EXPECT_FALSE(IsAnyOverlayOpen());
    }

    TEST_F(AlertTests, DestructiveActionIsNotTheDefault)
    {
        m_Options.IsDestructive = true;
        OpenAndSettle();
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Result, AlertResult::Secondary);
    }

    TEST_F(AlertTests, SingleButtonAnswersBothKeys)
    {
        m_Options.SecondaryLabel = {};
        m_Options.IsDestructive = true;
        OpenAndSettle();
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Result, AlertResult::Primary);

        m_Result = AlertResult::None;
        OpenAndSettle();
        TapKey(Key::Escape, Interface());
        EXPECT_EQ(m_Result, AlertResult::Primary);
    }

    TEST_F(AlertTests, TabReachesTheButtonsAndSpacePressesThem)
    {
        OpenAndSettle();
        TapKey(Key::Tab, Interface());
        TapKey(Key::Space, Interface());
        // The secondary button comes first, at the leading side.
        EXPECT_EQ(m_Result, AlertResult::Secondary);
    }

    TEST_F(AlertTests, IsModal)
    {
        OpenAndSettle();
        Click(Vec2(50.0f, 50.0f), Interface());
        EXPECT_TRUE(IsAnyOverlayOpen());
        EXPECT_EQ(m_UnderClicks, 0);
        EXPECT_EQ(m_Answers, 0);
    }

    TEST_F(AlertTests, SheetIsModalAndClosesWithEscape)
    {
        bool closeNow = false;
        SheetOptions options;
        const Builder build = [&]
        {
            m_Under = ButtonBehavior(HashID("under"), Rect(0.0f, 0.0f, 800.0f, 600.0f));
            m_UnderClicks += m_Under.Clicked ? 1 : 0;
            if (BeginSheet("sheet", options))
            {
                AllocateItem(Vec2(100.0f, 100.0f));
                if (closeNow)
                    CloseCurrentSheet();
                EndSheet();
            }
        };
        OpenSheet("sheet");
        Settle(build);
        EXPECT_TRUE(IsSheetOpen("sheet"));
        Click(Vec2(50.0f, 50.0f), build);
        EXPECT_TRUE(IsSheetOpen("sheet"));
        EXPECT_EQ(m_UnderClicks, 0);

        TapKey(Key::Escape, build);
        EXPECT_FALSE(IsSheetOpen("sheet"));

        options.DismissOnEscape = false;
        OpenSheet("sheet");
        Settle(build);
        TapKey(Key::Escape, build);
        EXPECT_TRUE(IsSheetOpen("sheet"));

        closeNow = true;
        Frame(build);
        EXPECT_FALSE(IsSheetOpen("sheet"));

        closeNow = false;
        OpenSheet("sheet");
        Settle(build);
        CloseSheet("sheet");
        EXPECT_FALSE(IsSheetOpen("sheet"));
    }
} // namespace Carbon
