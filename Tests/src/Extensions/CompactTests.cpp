#include <Carbon/Extensions/Extensions.h>

#include <string>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // The iOS patterns Carbon switches to in compact width: navigation stacks, sheets from the bottom, a "more"
    // menu in the menu bar, tables that scroll sideways.
    class CompactTests : public WidgetTest
    {
    protected:
        void SetUp() override
        {
            WidgetTest::SetUp();
            GetIO().SetDisplaySize(390.0f, 844.0f); // an iPhone in portrait
            GetIO().SetTouchModeOverride(true);
        }

        // A navigation split view: a sidebar of three pages and the selected page.
        Builder Navigation()
        {
            return [this]
            {
                BeginNavigationSplitView("main", {.Title = "Pages", .DetailTitle = Pages[m_Page]});
                BeginSidebar("pages");
                for (int i = 0; i < 3; i++)
                {
                    if (SidebarItem(Pages[i], m_Page == i))
                        m_Page = i;
                    m_Rows[i] = GetItemRect();
                }
                EndSidebar();
                NavigationSplitViewDetail();
                m_Content = AllocateItem(Vec2(100.0f, 40.0f));
                EndNavigationSplitView();
            };
        }

        static constexpr const char* Pages[3] = {"Inbox", "Sent", "Archive"};
        int m_Page = 0;
        Rect m_Rows[3];
        Rect m_Content;
    };

    TEST_F(CompactTests, TheSidebarBecomesARootListThatPushesTheContent)
    {
        Settle(Navigation(), 60);
        EXPECT_FALSE(IsNavigationDetailShown("main"));
        // The sidebar fills the width; the content waits off screen to the trailing side.
        EXPECT_NEAR(m_Rows[0].Width, 390.0f, 30.0f);
        EXPECT_GE(m_Content.X, 390.0f);

        Tap(m_Rows[1].GetCenter(), Navigation());
        EXPECT_EQ(m_Page, 1);
        EXPECT_TRUE(IsNavigationDetailShown("main"));
        Frame(Navigation());
        const float sliding = m_Content.X;
        EXPECT_GT(sliding, 0.0f); // it slides in on a spring
        EXPECT_LT(sliding, 390.0f);
        Settle(Navigation(), 60);
        EXPECT_FLOAT_EQ(m_Content.X, 0.0f);
        EXPECT_LT(m_Rows[0].X, -100.0f); // the list moved aside, under the content
    }

    TEST_F(CompactTests, TheBackButtonAndASwipeFromTheEdgeGoBack)
    {
        ShowNavigationDetail("main", true, false);
        Settle(Navigation(), 30);
        EXPECT_FLOAT_EQ(m_Content.X, 0.0f);

        // The back button sits at the leading end of the navigation bar.
        Tap(Vec2(30.0f, 22.0f), Navigation());
        EXPECT_FALSE(IsNavigationDetailShown("main"));
        Settle(Navigation(), 60);
        EXPECT_GE(m_Content.X, 390.0f);

        // A swipe from the leading edge follows the finger and goes back once past half the width.
        ShowNavigationDetail("main", true, false);
        Settle(Navigation(), 30);
        TouchDown(Vec2(5.0f, 400.0f), Navigation());
        TouchDrag(Vec2(5.0f, 400.0f), Vec2(105.0f, 400.0f), 5, Navigation());
        EXPECT_GT(m_Content.X, 50.0f); // following the finger
        EXPECT_LT(m_Content.X, 120.0f);
        TouchDrag(Vec2(105.0f, 400.0f), Vec2(300.0f, 400.0f), 5, Navigation());
        TouchUp(Vec2(300.0f, 400.0f), Navigation());
        EXPECT_FALSE(IsNavigationDetailShown("main"));

        // A short swipe returns to the content.
        ShowNavigationDetail("main", true, false);
        Settle(Navigation(), 30);
        TouchDown(Vec2(5.0f, 400.0f), Navigation());
        TouchDrag(Vec2(5.0f, 400.0f), Vec2(60.0f, 400.0f), 10, Navigation()); // slowly
        for (int i = 0; i < 6; i++)
            Frame(Navigation());
        TouchUp(Vec2(60.0f, 400.0f), Navigation());
        EXPECT_TRUE(IsNavigationDetailShown("main"));
        Settle(Navigation(), 60);
        EXPECT_FLOAT_EQ(m_Content.X, 0.0f);
    }

    TEST_F(CompactTests, TheListUnderTheContentTakesNoTaps)
    {
        ShowNavigationDetail("main", true, false);
        Settle(Navigation(), 30);
        // Empty space of the content, where the list lies beneath, moved aside.
        Tap(Vec2(150.0f, m_Rows[1].GetCenter().Y), Navigation());
        Tap(Vec2(150.0f, m_Rows[2].GetCenter().Y), Navigation());
        EXPECT_EQ(m_Page, 0);
        EXPECT_TRUE(IsNavigationDetailShown("main"));
    }

    TEST_F(CompactTests, InRegularWidthTheSidebarStandsNextToTheContent)
    {
        GetIO().SetDisplaySize(1024.0f, 768.0f);
        Settle(Navigation(), 30);
        EXPECT_FLOAT_EQ(m_Rows[0].X, 10.0f);  // the sidebar's padding
        EXPECT_FLOAT_EQ(m_Content.X, 220.0f); // right after the 220-point sidebar
        Tap(m_Rows[2].GetCenter(), Navigation());
        Settle(Navigation(), 30);
        EXPECT_EQ(m_Page, 2);
        EXPECT_FLOAT_EQ(m_Content.X, 220.0f);
    }

    TEST_F(CompactTests, ASplitViewCollapsesToo)
    {
        Rect first;
        Rect second;
        bool picked = false;
        const Builder build = [&]
        {
            BeginSplitView("split", {.Title = "Folders"});
            BeginList("folders", {.Height = 300.0f});
            picked = ListItem("Documents", false) || picked;
            first = GetItemRect();
            EndList();
            SplitViewDivider();
            second = AllocateItem(Vec2(80.0f, 30.0f));
            EndSplitView();
        };
        Settle(build, 30);
        EXPECT_GE(second.X, 390.0f);
        Tap(first.GetCenter(), build);
        EXPECT_TRUE(picked);
        Settle(build, 60);
        EXPECT_FLOAT_EQ(second.X, 0.0f);

        // A vertical split view stays as it is.
        Rect top;
        Rect bottom;
        const Builder vertical = [&]
        {
            BeginSplitView("stacked", {.Axis = Axis::Vertical, .InitialSize = 200.0f});
            top = AllocateItem(Vec2(80.0f, 30.0f));
            SplitViewDivider();
            bottom = AllocateItem(Vec2(80.0f, 30.0f));
            EndSplitView();
        };
        Settle(vertical, 30);
        EXPECT_FLOAT_EQ(top.Y, 0.0f);
        EXPECT_GT(bottom.Y, 200.0f);
    }

    TEST_F(CompactTests, ScrollingAListWithAFingerPicksNoRow)
    {
        int picked = -1;
        Rect first;
        const Builder build = [&]
        {
            BeginList("list", {.Height = 300.0f});
            for (int i = 0; i < 30; i++)
            {
                PushID(i);
                if (ListItem("Row", false))
                    picked = i;
                PopID();
                if (i == 0)
                    first = GetItemRect();
            }
            EndList();
        };
        Settle(build);
        TouchDown(first.GetCenter() + Vec2(0.0f, 100.0f), build);
        TouchDrag(first.GetCenter() + Vec2(0.0f, 100.0f), first.GetCenter(), 5, build);
        TouchUp(first.GetCenter(), build);
        EXPECT_EQ(picked, -1);
        Settle(build, 120);
        Tap(Vec2(100.0f, 150.0f), build);
        EXPECT_GE(picked, 0); // a tap picks one
    }

    // ---- Sheets ---------------------------------------------------------------------------------------------------

    class SheetPresentationTests : public CompactTests
    {
    protected:
        Builder MenuInterface()
        {
            return [this]
            {
                SetCursorPos(Vec2(20.0f, 100.0f));
                if (Button("Edit"))
                    OpenMenu("edit");
                if (BeginMenu("edit"))
                {
                    m_IsSheet = IsOverlayPresentedAsSheet();
                    if (MenuItem("Copy", {.Shortcut = "Ctrl+C"}))
                        m_Chosen = 1;
                    m_Item = GetItemRect();
                    EndMenu();
                }
            };
        }

        int m_Chosen = 0;
        bool m_IsSheet = false;
        Rect m_Item;
    };

    TEST_F(SheetPresentationTests, MenusSlideUpFromTheBottomAcrossTheWidth)
    {
        Settle(MenuInterface());
        Tap(Vec2(40.0f, 112.0f), MenuInterface());
        ASSERT_TRUE(IsMenuOpen("edit"));
        Frame(MenuInterface());
        Frame(MenuInterface());
        const float rising = m_Item.Y;
        Settle(MenuInterface(), 60);
        EXPECT_TRUE(m_IsSheet);
        EXPECT_LT(m_Item.Y, rising);     // it slid up
        EXPECT_GT(m_Item.Y, 700.0f);     // to the bottom of the display
        EXPECT_GT(m_Item.Width, 340.0f); // and its rows span it

        Tap(m_Item.GetCenter(), MenuInterface());
        EXPECT_EQ(m_Chosen, 1);
        EXPECT_FALSE(IsMenuOpen("edit"));
    }

    TEST_F(SheetPresentationTests, DraggingTheSheetDownDismissesIt)
    {
        Settle(MenuInterface());
        Tap(Vec2(40.0f, 112.0f), MenuInterface());
        Settle(MenuInterface(), 60);
        const Vec2 grabber(195.0f, m_Item.Y - 12.0f);
        TouchDown(grabber, MenuInterface());
        TouchDrag(grabber, grabber + Vec2(0.0f, 120.0f), 6, MenuInterface());
        TouchUp(grabber + Vec2(0.0f, 120.0f), MenuInterface());
        Settle(MenuInterface(), 60);
        EXPECT_FALSE(IsMenuOpen("edit"));
        EXPECT_EQ(m_Chosen, 0);

        // A tap above the sheet dismisses it too, after it slid out.
        Tap(Vec2(40.0f, 112.0f), MenuInterface());
        Settle(MenuInterface(), 60);
        ASSERT_TRUE(IsMenuOpen("edit"));
        Tap(Vec2(200.0f, 300.0f), MenuInterface());
        Settle(MenuInterface(), 60);
        EXPECT_FALSE(IsMenuOpen("edit"));
    }

    TEST_F(SheetPresentationTests, InRegularWidthMenusStayAnchored)
    {
        GetIO().SetDisplaySize(1024.0f, 768.0f);
        GetIO().SetTouchModeOverride(false);
        Settle(MenuInterface());
        Click(Vec2(40.0f, 112.0f), MenuInterface());
        Settle(MenuInterface(), 30);
        EXPECT_FALSE(m_IsSheet);
        EXPECT_LT(m_Item.Y, 200.0f); // below its button
        EXPECT_LT(m_Item.Width, 300.0f);
    }

    // ---- Menu bar and table ---------------------------------------------------------------------------------------

    TEST_F(CompactTests, MenusThatDoNotFitMoveIntoAMoreMenu)
    {
        static constexpr const char* Titles[] = {"File", "Edit", "View", "Window", "Format", "Arrange", "Help"};
        int chosen = -1;
        Rect lastTitle;
        Rect deepItem;
        const Builder build = [&]
        {
            BeginMenuBar({.Width = Size::Fixed(300.0f)});
            for (int i = 0; i < 7; i++)
            {
                if (BeginMenuBarMenu(Titles[i]))
                {
                    if (MenuItem("Command"))
                        chosen = i;
                    deepItem = GetItemRect();
                    EndMenuBarMenu();
                }
                lastTitle = GetItemRect();
            }
            EndMenuBar();
        };
        Settle(build);
        // The bar keeps its titles within its width and ends with the "more" button.
        EXPECT_LE(lastTitle.GetRight(), 300.0f);
        Tap(lastTitle.GetCenter(), build);
        Settle(build, 60);
        EXPECT_TRUE(IsAnyOverlayOpen());
    }

    TEST_F(CompactTests, TablesScrollSidewaysInsteadOfSqueezing)
    {
        static const TableColumn Columns[] = {
            {.Title = "Name"}, {.Title = "Kind"}, {.Title = "Size"}, {.Title = "Date"}};
        Rect cell;
        const Builder build = [&]
        {
            BeginTable("files", Columns, {.Width = Size::Fixed(300.0f), .Height = 200.0f});
            TableRow("row");
            BeginTableCell();
            cell = AllocateItem(Vec2(40.0f, 20.0f));
            EndTableCell();
            TableCell("PDF");
            TableCell("1 MB");
            TableCell("Today");
            EndTable();
        };
        Settle(build);
        const float before = cell.X;
        TouchDown(Vec2(250.0f, 60.0f), build);
        TouchDrag(Vec2(250.0f, 60.0f), Vec2(130.0f, 60.0f), 6, build);
        TouchUp(Vec2(130.0f, 60.0f), build);
        Settle(build, 120);
        EXPECT_LT(cell.X, before - 50.0f);
    }
} // namespace Carbon
