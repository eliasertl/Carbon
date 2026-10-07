#include <Carbon/Extensions/Extensions.h>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // ---- Sidebar ------------------------------------------------------------------------------------------------

    class SidebarTests : public WidgetTest
    {
    protected:
        static constexpr int Count = 5;

        Builder Interface()
        {
            return [this]
            {
                static const char* const Names[Count] = {"Inbox", "Drafts", "Sent", "Archive", "Trash"};
                BeginHStack({.Width = Size::Fill(), .Height = Size::Fill()});
                BeginSidebar("sidebar");
                SidebarHeader("Mailboxes");
                for (int i = 0; i < Count; i++)
                {
                    SidebarItemOptions options;
                    options.Disabled = i == m_DisabledIndex;
                    if (SidebarItem(Names[i], i == m_Selected, options))
                    {
                        m_Selected = i;
                        m_Picks++;
                    }
                    m_Rects[i] = GetItemRect();
                }
                EndSidebar();
                m_Content = AllocateItem(Vec2(10.0f, 10.0f));
                EndHStack();
            };
        }

        int m_Selected = 0;
        int m_Picks = 0;
        int m_DisabledIndex = -1;
        Rect m_Rects[Count];
        Rect m_Content;
    };

    TEST_F(SidebarTests, TakesItsWidthAndTheFullHeight)
    {
        Settle(Interface());
        // 220 points wide; the content starts right after it.
        EXPECT_FLOAT_EQ(m_Content.X, 220.0f + 8.0f);
        // Items span the sidebar's width minus its padding of 10.
        EXPECT_FLOAT_EQ(m_Rects[0].X, 10.0f);
        EXPECT_FLOAT_EQ(m_Rects[0].Width, 200.0f);
        EXPECT_FLOAT_EQ(m_Rects[0].Height, 28.0f);
    }

    TEST_F(SidebarTests, ClickPicksAnItem)
    {
        Settle(Interface());
        Click(m_Rects[3].GetCenter(), Interface());
        EXPECT_EQ(m_Selected, 3);
        EXPECT_EQ(m_Picks, 1);
    }

    TEST_F(SidebarTests, ArrowKeysMoveTheSelectionWhileFocused)
    {
        Settle(Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 0) << "without focus the keys do nothing";

        Click(m_Rects[3].GetCenter(), Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 4);
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 4) << "the selection stops at the last item";
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Selected, 3);
        TapKey(Key::Home, Interface());
        EXPECT_EQ(m_Selected, 0);
        TapKey(Key::End, Interface());
        EXPECT_EQ(m_Selected, 4);
    }

    TEST_F(SidebarTests, IsOneTabStop)
    {
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        const ID first = GetFocusedID();
        EXPECT_TRUE(first.IsValid());
        TapKey(Key::Tab, Interface());
        EXPECT_EQ(GetFocusedID(), first) << "the items are not stops of their own";
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 1);
    }

    TEST_F(SidebarTests, DisabledItemsAreSkipped)
    {
        m_DisabledIndex = 1;
        Settle(Interface());
        Click(m_Rects[1].GetCenter(), Interface());
        EXPECT_EQ(m_Selected, 0);

        Click(m_Rects[0].GetCenter(), Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 2);
    }

    TEST_F(SidebarTests, HighlightSlidesAndSettles)
    {
        Settle(Interface());
        EXPECT_FALSE(IsAnimating());
        Click(m_Rects[4].GetCenter(), Interface());
        // The application selects the item; from the next frame on the highlight is on its way.
        Frame(Interface());
        EXPECT_TRUE(IsAnimating());
        Settle(Interface(), 120);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(SidebarTests, HighlightSlidesUpwardsToo)
    {
        // Picking a row above the selected one changes the application's selection after the new row was
        // submitted and before the old one is: for one frame no row is marked as selected.
        m_Selected = 4;
        Settle(Interface(), 60);
        // Shapes in order: background, trailing hairline, highlight.
        const auto getHighlightTop = [] { return GetDrawData().Vertices[8].Position.Y + 1.0f; };
        EXPECT_FLOAT_EQ(getHighlightTop(), m_Rects[4].Y);

        MoveMouse(m_Rects[0].GetCenter(), Interface());
        PressMouse(Interface());
        EXPECT_EQ(m_Selected, 0) << "a row is picked when the button goes down";
        EXPECT_FLOAT_EQ(getHighlightTop(), m_Rects[4].Y) << "the highlight stays while the selection changes hands";

        ReleaseMouse(Interface());
        Frame(Interface());
        EXPECT_LT(getHighlightTop(), m_Rects[4].Y);
        EXPECT_GT(getHighlightTop(), m_Rects[0].Y) << "it slides, it does not jump";

        Settle(Interface(), 120);
        EXPECT_NEAR(getHighlightTop(), m_Rects[0].Y, 0.01f);
    }

    TEST_F(SidebarTests, RowUnderThePointerIsTinted)
    {
        Settle(Interface());
        const size_t before = GetDrawData().Vertices.size();
        MoveMouse(m_Rects[2].GetCenter(), Interface());
        Settle(Interface());
        // One more shape than without the pointer: the tint behind the hovered row.
        EXPECT_EQ(GetDrawData().Vertices.size(), before + 4);

        // The selected row has its highlight and gets no tint.
        MoveMouse(m_Rects[0].GetCenter(), Interface());
        Settle(Interface(), 60);
        EXPECT_EQ(GetDrawData().Vertices.size(), before);
    }

    // ---- List ---------------------------------------------------------------------------------------------------

    class ListTests : public WidgetTest
    {
    protected:
        static constexpr int Count = 20;

        Builder Interface()
        {
            return [this]
            {
                BeginList("list", {.Width = 300.0f, .Height = 110.0f});
                for (int i = 0; i < Count; i++)
                {
                    PushID(i);
                    if (ListItem("Row", i == m_Selected))
                        m_Selected = i;
                    if (i < 3)
                        m_Rects[i] = GetItemRect();
                    PopID();
                }
                EndList();
            };
        }

        int m_Selected = 0;
        Rect m_Rects[3];
    };

    TEST_F(ListTests, RowsSpanTheListInsideItsPadding)
    {
        Settle(Interface());
        EXPECT_EQ(m_Rects[0], Rect(5.0f, 5.0f, 290.0f, 24.0f));
        EXPECT_FLOAT_EQ(m_Rects[1].Y, 29.0f);
    }

    TEST_F(ListTests, ClickAndKeysSelect)
    {
        Settle(Interface());
        Click(m_Rects[2].GetCenter(), Interface());
        EXPECT_EQ(m_Selected, 2);
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Selected, 1);
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 3);
    }

    TEST_F(ListTests, KeyboardSelectionIsScrolledIntoView)
    {
        Settle(Interface());
        Click(m_Rects[0].GetCenter(), Interface());
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, 0.0f);

        TapKey(Key::End, Interface());
        EXPECT_EQ(m_Selected, Count - 1);
        Settle(Interface(), 60);
        // 20 rows of 24 points and 10 points of padding in a view of 110 points: scrolled to the end, give or
        // take the difference between the padding and the margin kept around a revealed row.
        EXPECT_NEAR(GetScrollOffset("list").Y, 20.0f * 24.0f + 10.0f - 110.0f, 1.5f);

        TapKey(Key::Home, Interface());
        Settle(Interface(), 60);
        EXPECT_EQ(m_Selected, 0);
        EXPECT_NEAR(GetScrollOffset("list").Y, 0.0f, 1.5f);
    }

    TEST_F(ListTests, RowOutsideAListIsReported)
    {
        Frame([] { ListItem("Stray", false); });
        EXPECT_FALSE(m_AssertMessages.empty());
    }

    TEST_F(ListTests, RowsOutOfViewAreNotInteractive)
    {
        Settle(Interface());
        // The seventh row lies below the list's 110 points. Its place on the display belongs to nobody.
        Click(Vec2(150.0f, 5.0f + 24.0f * 6.0f + 12.0f), Interface());
        EXPECT_EQ(m_Selected, 0);
    }

    // A list of 100,000 items of which the application submits only the ones ClipListItems asks for.
    class ClippedListTests : public WidgetTest
    {
    protected:
        static constexpr int Count = 100000;
        // The list's content is its rows and 10 points of padding; 110 points of it are visible.
        static constexpr float MaxOffset = 24.0f * Count + 10.0f - 110.0f;

        Builder Interface()
        {
            return [this]
            {
                BeginList("list", {.Width = 300.0f, .Height = 110.0f});
                const RowRange items = ClipListItems(Count, m_Selected);
                m_Range = items;
                for (int i = items.First; i < items.End; i++)
                {
                    PushID(i);
                    if (ListItem("Row", i == m_Selected))
                        m_Selected = i;
                    if (i == m_Watched)
                        m_WatchedRect = GetItemRect();
                    PopID();
                }
                EndList();
            };
        }

        void ScrollTo(float offset)
        {
            Frame(
                [&]
                {
                    SetScrollOffset("list", Vec2(0.0f, offset));
                    Interface()();
                });
            Settle(Interface(), 60);
        }

        int m_Selected = -1;
        int m_Watched = 0;
        RowRange m_Range;
        Rect m_WatchedRect;
    };

    TEST_F(ClippedListTests, OnlyTheVisibleItemsAreSubmitted)
    {
        Settle(Interface());
        EXPECT_EQ(m_Range.First, 0);
        // Four rows and part of a fifth are visible, and one more is asked for on either side.
        EXPECT_GE(m_Range.End, 5);
        EXPECT_LE(m_Range.End, 7);
        EXPECT_EQ(m_WatchedRect, Rect(5.0f, 5.0f, 290.0f, 24.0f));
    }

    TEST_F(ClippedListTests, ScrollsAsIfEveryItemWereThere)
    {
        Settle(Interface());
        ScrollTo(1.0e9f);
        EXPECT_FLOAT_EQ(GetScrollOffset("list").Y, MaxOffset);
        EXPECT_EQ(m_Range.End, Count);
        EXPECT_GE(m_Range.First, Count - 7);

        // An item in the middle sits where it would with every item before it submitted.
        m_Watched = 50000;
        ScrollTo(24.0f * 50000.0f);
        EXPECT_LE(m_Range.First, 50000);
        EXPECT_GT(m_Range.End, 50000);
        EXPECT_FLOAT_EQ(m_WatchedRect.Y, 5.0f);
        EXPECT_FLOAT_EQ(m_WatchedRect.Height, 24.0f);
    }

    TEST_F(ClippedListTests, KeyboardMovesTheSelectionAndRevealsIt)
    {
        Settle(Interface());
        Click(Vec2(150.0f, 5.0f + 12.0f), Interface());
        EXPECT_EQ(m_Selected, 0);
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 1);

        TapKey(Key::End, Interface());
        EXPECT_EQ(m_Selected, Count - 1);
        Settle(Interface(), 60);
        EXPECT_NEAR(GetScrollOffset("list").Y, MaxOffset, 1.5f);

        TapKey(Key::Home, Interface());
        EXPECT_EQ(m_Selected, 0);
        Settle(Interface(), 60);
        EXPECT_NEAR(GetScrollOffset("list").Y, 0.0f, 1.5f);
    }

    TEST_F(ClippedListTests, KeyboardStartsFromASelectionThatIsNotSubmitted)
    {
        Settle(Interface());
        Click(Vec2(150.0f, 5.0f + 12.0f), Interface());
        // The application selects an item far away; the list stays where it is and still has the focus.
        m_Selected = 70000;
        Settle(Interface());
        EXPECT_LT(m_Range.End, 70000);

        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 70001);
        Settle(Interface(), 60);
        // The new selection has been scrolled to the bottom edge of the visible area.
        EXPECT_NEAR(GetScrollOffset("list").Y, 24.0f * 70002.0f + 10.0f - 110.0f, 6.0f);
    }

    // ---- Table --------------------------------------------------------------------------------------------------

    class TableTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                const TableColumn columns[] = {{.Title = "Done", .Width = 100.0f},
                                               {.Title = "Name"},
                                               {.Title = "Size", .Width = Size::Fill(2.0f)}};
                TableOptions options;
                options.Width = 400.0f;
                options.Height = 200.0f;
                options.ShowsHeader = m_ShowsHeader;
                BeginTable("table", columns, options);
                for (int i = 0; i < 3; i++)
                {
                    if (TableRow(i, i == m_Selected))
                        m_Selected = i;
                    BeginTableCell();
                    Toggle("##done", &m_Done[i], {.Kind = ToggleKind::Checkbox});
                    m_ToggleIDs[i] = GetItemID();
                    m_Cells[i][0] = GetItemRect();
                    EndTableCell();
                    BeginTableCell();
                    m_Cells[i][1] = AllocateItem(Vec2(10.0f, 10.0f));
                    EndTableCell();
                    TableCell("42 KB");
                    for (int extra = 0; extra < m_ExtraCells; extra++)
                        TableCell("too many");
                }
                EndTable();
                m_Table = GetLastItemRect();
            };
        }

        int m_Selected = -1;
        bool m_Done[3] = {false, false, false};
        bool m_ShowsHeader = true;
        int m_ExtraCells = 0;
        ID m_ToggleIDs[3];
        Rect m_Cells[3][2];
        Rect m_Table;
    };

    TEST_F(TableTests, ColumnsShareTheWidth)
    {
        Settle(Interface());
        EXPECT_EQ(m_Table, Rect(0.0f, 0.0f, 400.0f, 200.0f));
        // Rows are inset by 5 points and cells padded by 8. The fixed column takes 100 of the 390 points; the
        // other two share the rest 1 : 2.
        EXPECT_FLOAT_EQ(m_Cells[0][0].X, 5.0f + 8.0f);
        EXPECT_NEAR(m_Cells[0][1].X, 5.0f + 100.0f + 8.0f, 0.5f);
        // Header 24, then 3 points of padding above the rows of 24 points.
        EXPECT_NEAR(m_Cells[0][1].GetCenter().Y, 24.0f + 3.0f + 12.0f, 0.5f);
        EXPECT_NEAR(m_Cells[1][1].GetCenter().Y, 24.0f + 3.0f + 36.0f, 0.5f);
    }

    TEST_F(TableTests, HeaderCanBeHidden)
    {
        m_ShowsHeader = false;
        Settle(Interface());
        EXPECT_NEAR(m_Cells[0][1].GetCenter().Y, 3.0f + 12.0f, 0.5f);
        EXPECT_NEAR(m_Cells[0][1].X, 5.0f + 100.0f + 8.0f, 0.5f);
    }

    TEST_F(TableTests, ClippedRowsKeepTheirPlace)
    {
        // 1,000 rows, of which the ones in view are submitted: a row is where it would be among all of them.
        const TableColumn columns[] = {{.Title = "Name"}};
        RowRange range;
        Rect rowRect;
        const auto build = [&]
        {
            BeginTable("table", columns, {.Width = 400.0f, .Height = 200.0f});
            range = ClipTableRows(1000, 501);
            for (int i = range.First; i < range.End; i++)
            {
                TableRow(i, i == 501);
                if (i == 500)
                    rowRect = GetItemRect();
                TableCell("Name");
            }
            EndTable();
        };
        Settle(build);
        Frame(
            [&]
            {
                PushID("table");
                SetScrollOffset("##rows", Vec2(0.0f, 24.0f * 500.0f));
                PopID();
                build();
            });
        Settle(build, 60);
        EXPECT_LE(range.First, 500);
        EXPECT_LT(range.End - range.First, 12);
        // Header 24, then 3 points of padding above the rows; row 500 is the first one in view.
        EXPECT_FLOAT_EQ(rowRect.Y, 24.0f + 3.0f);
    }

    TEST_F(TableTests, ClickingARowPicksIt)
    {
        Settle(Interface());
        Click(Vec2(300.0f, m_Cells[1][1].GetCenter().Y), Interface());
        EXPECT_EQ(m_Selected, 1);
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 2);
    }

    TEST_F(TableTests, WidgetsInCellsBelongToTheirRow)
    {
        Settle(Interface());
        EXPECT_NE(m_ToggleIDs[0], m_ToggleIDs[1]);
        Click(m_Cells[1][0].GetCenter(), Interface());
        EXPECT_FALSE(m_Done[0]);
        EXPECT_TRUE(m_Done[1]);
        EXPECT_EQ(m_Selected, -1) << "the checkbox takes the click, not the row";
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(TableTests, TooManyCellsAreReported)
    {
        m_ExtraCells = 1;
        Frame(Interface());
        ASSERT_FALSE(m_AssertMessages.empty());
        EXPECT_NE(m_AssertMessages[0].find("columns"), std::string::npos);
    }

    // ---- TabView ------------------------------------------------------------------------------------------------

    class TabViewTests : public WidgetTest
    {
    };

    TEST_F(TabViewTests, SwitchesPanes)
    {
        int tab = 0;
        Rect tabs;
        Rect content;
        const Builder build = [&]
        {
            BeginTabView("tabs", &tab, {"First", "Second"});
            tabs = GetItemRect();
            content = AllocateItem(Vec2(50.0f, tab == 0 ? 40.0f : 80.0f));
            EndTabView();
        };
        Settle(build);
        // The tabs are centered above the content area, which is padded by 16 points.
        EXPECT_NEAR(tabs.GetCenter().X, 400.0f, 0.5f);
        EXPECT_FLOAT_EQ(content.X, 16.0f);
        EXPECT_FLOAT_EQ(content.Y, tabs.GetBottom() + 10.0f + 16.0f);
        EXPECT_FLOAT_EQ(content.Height, 40.0f);

        Click(Vec2(tabs.X + tabs.Width * 0.75f, tabs.GetCenter().Y), build);
        EXPECT_EQ(tab, 1);
        Settle(build);
        EXPECT_FLOAT_EQ(content.Height, 80.0f);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    // ---- SplitView ----------------------------------------------------------------------------------------------

    class SplitViewTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                BeginVStack({.Width = 600.0f, .Height = 300.0f});
                BeginSplitView("split", m_Options);
                m_First = AllocateItem(Vec2(10.0f, 10.0f), {.Width = Size::Fill()});
                SplitViewDivider();
                m_Second = AllocateItem(Vec2(10.0f, 10.0f), {.Width = Size::Fill()});
                EndSplitView();
                EndVStack();
            };
        }

        void Drag(Vec2 from, Vec2 to)
        {
            MoveMouse(from, Interface());
            PressMouse(Interface());
            GetIO().AddMousePosEvent(to.X, to.Y);
            Frame(Interface());
            ReleaseMouse(Interface());
            Settle(Interface());
        }

        SplitViewOptions m_Options = {.InitialSize = 200.0f, .MinSize = 100.0f, .MinSecondSize = 150.0f};
        Rect m_First;
        Rect m_Second;
    };

    TEST_F(SplitViewTests, PanesShareTheSpace)
    {
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_First.X, 0.0f);
        EXPECT_FLOAT_EQ(m_First.Width, 200.0f);
        // The divider is one pixel wide.
        EXPECT_FLOAT_EQ(m_Second.X, 201.0f);
        EXPECT_FLOAT_EQ(m_Second.Width, 399.0f);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(SplitViewTests, DraggingTheDividerResizes)
    {
        Settle(Interface());
        Drag(Vec2(200.5f, 150.0f), Vec2(260.5f, 150.0f));
        EXPECT_FLOAT_EQ(m_First.Width, 260.0f);
        EXPECT_FLOAT_EQ(m_Second.X, 261.0f);
    }

    TEST_F(SplitViewTests, PanesKeepTheirMinimumSizes)
    {
        Settle(Interface());
        Drag(Vec2(200.5f, 150.0f), Vec2(10.0f, 150.0f));
        EXPECT_FLOAT_EQ(m_First.Width, 100.0f);

        Drag(Vec2(100.5f, 150.0f), Vec2(790.0f, 150.0f));
        EXPECT_FLOAT_EQ(m_First.Width, 450.0f);
    }

    TEST_F(SplitViewTests, ArrowKeysMoveTheFocusedDivider)
    {
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        TapKey(Key::RightArrow, Interface());
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_First.Width, 212.0f);
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::LeftArrow, Interface());
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_First.Width, 188.0f);
    }

    TEST_F(SplitViewTests, VerticalSplitStacksThePanes)
    {
        m_Options.Axis = Axis::Vertical;
        m_Options.InitialSize = 120.0f;
        m_Options.MinSecondSize = 50.0f;
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_First.Y, 0.0f);
        EXPECT_FLOAT_EQ(m_Second.Y, 121.0f);
        EXPECT_FLOAT_EQ(m_Second.Width, 600.0f);
    }

    TEST_F(SplitViewTests, MisuseIsReported)
    {
        Frame([] { SplitViewDivider(); });
        EXPECT_EQ(m_AssertMessages.size(), 1u);
        Frame([] { EndSplitView(); });
        EXPECT_EQ(m_AssertMessages.size(), 2u);
    }

    // ---- Charts -------------------------------------------------------------------------------------------------

    class ChartTests : public WidgetTest
    {
    };

    TEST_F(ChartTests, DrawsAndSettles)
    {
        const float values[] = {1.0f, 4.0f, 2.0f, 8.0f};
        const float other[] = {2.0f, 3.0f, 5.0f};
        const std::string_view labels[] = {"A", "B", "C", "D"};
        const ChartSeries series[] = {{.Label = "One", .Values = values}, {.Label = "Two", .Values = other}};
        Rect line;
        Rect bar;
        const Builder build = [&]
        {
            LineChart("line", series, {.Height = 150.0f, .Labels = labels});
            line = GetItemRect();
            BarChart("bar", series, {.Width = 300.0f, .Height = 120.0f, .Labels = labels});
            bar = GetItemRect();
        };
        Frame(build);
        EXPECT_TRUE(IsAnimating()) << "charts draw themselves in";
        Settle(build, 240);
        EXPECT_FALSE(IsAnimating());
        EXPECT_EQ(line, Rect(0.0f, 0.0f, 800.0f, 150.0f));
        EXPECT_EQ(bar.GetSize(), Vec2(300.0f, 120.0f));
        EXPECT_GT(GetDrawData().Vertices.size(), 40u);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(ChartTests, EmptyAndDegenerateDataAreHarmless)
    {
        const float flat[] = {3.0f, 3.0f, 3.0f};
        const float single[] = {5.0f};
        const float negative[] = {-4.0f, 2.0f, -1.0f};
        const Builder build = [&]
        {
            LineChart("none", std::span<const ChartSeries>());
            BarChart("none bars", std::span<const ChartSeries>());
            LineChart("flat", flat);
            LineChart("single", single);
            BarChart("negative", negative);
            BarChart("tiny", flat, {.Width = 4.0f, .Height = 4.0f});
        };
        Settle(build, 60);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(ChartTests, PointerCallsOutTheNearestValue)
    {
        const float values[] = {1.0f, 4.0f, 2.0f, 8.0f};
        size_t calloutIndices = 0;
        const Builder build = [&]
        {
            LineChart("line", values, {.Height = 200.0f});
            calloutIndices = GetDrawList().GetIndices(DrawLayer::Tooltip).size();
        };
        Settle(build, 120);
        EXPECT_EQ(calloutIndices, 0u);

        MoveMouse(Vec2(400.0f, 100.0f), build);
        EXPECT_GT(calloutIndices, 0u);

        MoveMouse(Vec2(400.0f, 400.0f), build);
        EXPECT_EQ(calloutIndices, 0u);
    }
} // namespace Carbon
