#include <Carbon/Extensions/Extensions.h>

#include <vector>

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

    // ---- Reordering lists ---------------------------------------------------------------------------------------

    TEST(ListMoveTests, ApplyingAMoveShiftsTheItemsBetween)
    {
        std::vector<char> items = {'A', 'B', 'C', 'D', 'E'};
        ApplyListMove(items, {.From = 0, .To = 2});
        EXPECT_EQ(items, (std::vector<char>{'B', 'C', 'A', 'D', 'E'}));
        ApplyListMove(items, {.From = 4, .To = 1});
        EXPECT_EQ(items, (std::vector<char>{'B', 'E', 'C', 'A', 'D'}));
        // Nothing moved, or a move that does not fit the items, changes nothing.
        ApplyListMove(items, {});
        ApplyListMove(items, {.From = 2, .To = 2});
        ApplyListMove(items, {.From = 1, .To = 9});
        EXPECT_EQ(items, (std::vector<char>{'B', 'E', 'C', 'A', 'D'}));
        EXPECT_FALSE((ListMove{.From = 3, .To = 3}.IsMoved()));
    }

    class ListReorderTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                BeginList("list", {.Width = 300.0f, .Height = 200.0f, .AllowsReordering = m_AllowsReordering});
                for (size_t i = 0; i < m_Items.size(); i++)
                {
                    const std::string_view name(&m_Items[i], 1);
                    // The ID follows the item, not its position.
                    PushID(name);
                    ListItem(name, false);
                    m_Rects[m_Items[i] - 'A'] = GetItemRect();
                    PopID();
                }
                const ListMove move = EndList();
                if (move.IsMoved())
                {
                    m_Moves++;
                    m_LastMove = move;
                    ApplyListMove(m_Items, move);
                }
            };
        }

        /// The layout puts row i at 5 + 24 i; its middle is 12 points below that.
        static float RowMiddle(int row) { return 5.0f + 24.0f * static_cast<float>(row) + 12.0f; }

        bool m_AllowsReordering = true;
        std::vector<char> m_Items = {'A', 'B', 'C', 'D', 'E'};
        Rect m_Rects[5];
        int m_Moves = 0;
        ListMove m_LastMove;
    };

    TEST_F(ListReorderTests, DraggingAnItemMovesIt)
    {
        Settle(Interface());
        MoveMouse(Vec2(100.0f, RowMiddle(0)), Interface());
        PressMouse(Interface());
        // Between C and D: above the middle of D.
        MoveMouse(Vec2(100.0f, RowMiddle(3) - 6.0f), Interface());
        Settle(Interface(), 30);
        ASSERT_TRUE(IsDragging());
        // D and E made room below the insertion point; A, B and C stayed.
        EXPECT_NEAR(m_Rects['D' - 'A'].Y, 5.0f + 24.0f * 4.0f, 0.5f);
        EXPECT_NEAR(m_Rects['E' - 'A'].Y, 5.0f + 24.0f * 5.0f, 0.5f);
        EXPECT_NEAR(m_Rects['C' - 'A'].Y, 5.0f + 24.0f * 2.0f, 0.5f);
        EXPECT_EQ(m_Moves, 0);

        ReleaseMouse(Interface());
        EXPECT_EQ(m_Moves, 1);
        EXPECT_EQ(m_LastMove.From, 0);
        EXPECT_EQ(m_LastMove.To, 2);
        EXPECT_EQ(m_Items, (std::vector<char>{'B', 'C', 'A', 'D', 'E'}));

        // The rows slide into their new places rather than jumping there: A starts where it was drawn.
        Frame(Interface());
        EXPECT_LT(m_Rects['A' - 'A'].Y, 5.0f + 24.0f * 2.0f - 1.0f);
        EXPECT_GT(m_Rects['D' - 'A'].Y, 5.0f + 24.0f * 3.0f + 1.0f);
        Settle(Interface(), 60);
        EXPECT_NEAR(m_Rects['A' - 'A'].Y, 5.0f + 24.0f * 2.0f, 0.5f);
        EXPECT_NEAR(m_Rects['D' - 'A'].Y, 5.0f + 24.0f * 3.0f, 0.5f);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(ListReorderTests, DraggingToTheEndAndUp)
    {
        Settle(Interface());
        MoveMouse(Vec2(100.0f, RowMiddle(1)), Interface());
        PressMouse(Interface());
        // Below the last row.
        MoveMouse(Vec2(100.0f, 180.0f), Interface());
        Settle(Interface(), 5);
        ReleaseMouse(Interface());
        EXPECT_EQ(m_Items, (std::vector<char>{'A', 'C', 'D', 'E', 'B'}));
        Settle(Interface(), 60);

        // E, now fourth, goes to the top.
        MoveMouse(Vec2(100.0f, RowMiddle(3)), Interface());
        PressMouse(Interface());
        MoveMouse(Vec2(100.0f, 6.0f), Interface());
        Settle(Interface(), 5);
        ReleaseMouse(Interface());
        EXPECT_EQ(m_Items, (std::vector<char>{'E', 'A', 'C', 'D', 'B'}));
    }

    TEST_F(ListReorderTests, DroppingNextToItsPlaceMovesNothing)
    {
        Settle(Interface());
        MoveMouse(Vec2(100.0f, RowMiddle(2)), Interface());
        PressMouse(Interface());
        // Just below C's own middle: the slot after C, which is where C is.
        MoveMouse(Vec2(100.0f, RowMiddle(2) + 8.0f), Interface());
        Settle(Interface(), 5);
        ReleaseMouse(Interface());
        EXPECT_EQ(m_Moves, 0);
        Settle(Interface(), 60);
        EXPECT_NEAR(m_Rects['D' - 'A'].Y, 5.0f + 24.0f * 3.0f, 0.5f) << "the room closes again";
    }

    TEST_F(ListReorderTests, EscapeLeavesTheOrder)
    {
        Settle(Interface());
        MoveMouse(Vec2(100.0f, RowMiddle(0)), Interface());
        PressMouse(Interface());
        MoveMouse(Vec2(100.0f, RowMiddle(4)), Interface());
        TapKey(Key::Escape, Interface());
        ReleaseMouse(Interface());
        EXPECT_EQ(m_Moves, 0);
        EXPECT_EQ(m_Items, (std::vector<char>{'A', 'B', 'C', 'D', 'E'}));
    }

    TEST_F(ListReorderTests, AListWithoutReorderingDoesNotDrag)
    {
        m_AllowsReordering = false;
        Settle(Interface());
        MoveMouse(Vec2(100.0f, RowMiddle(0)), Interface());
        PressMouse(Interface());
        MoveMouse(Vec2(100.0f, RowMiddle(4)), Interface());
        EXPECT_FALSE(IsDragging());
        ReleaseMouse(Interface());
        EXPECT_EQ(m_Moves, 0);
    }

    // ---- Table --------------------------------------------------------------------------------------------------

    class TableTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                const TableColumn columns[] = {
                    {.Title = "Done", .Width = 100.0f, .IsSortable = false},
                    {.Title = "Name"},
                    {.Title = "Size", .Width = Size::Fill(2.0f), .InitialSortDirection = SortDirection::Descending}};
                TableOptions options;
                options.Width = 400.0f;
                options.Height = 200.0f;
                options.ShowsHeader = m_ShowsHeader;
                if (m_HasArrangement)
                {
                    options.ColumnWidths = m_Widths;
                    options.ColumnOrder = m_Order;
                }
                if (m_IsSorting)
                    options.Sort = &m_Sort;
                const TableChanges changes = BeginTable("table", columns, options);
                m_WidthsChanged = m_WidthsChanged || changes.WidthsChanged;
                m_OrderChanged = m_OrderChanged || changes.OrderChanged;
                m_SortChanges += changes.SortChanged ? 1 : 0;
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
                    TableCell(m_SizeText);
                    for (int extra = 0; extra < m_ExtraCells; extra++)
                        TableCell("too many");
                }
                EndTable();
                m_Table = GetLastItemRect();
            };
        }

        /// Drags with the left button from one point to another, in two steps.
        void Drag(Vec2 from, Vec2 to, const Builder& build)
        {
            MoveMouse(from, build);
            PressMouse(build);
            MoveMouse(from + (to - from) * 0.5f, build);
            MoveMouse(to, build);
            ReleaseMouse(build);
        }

        int m_Selected = -1;
        bool m_Done[3] = {false, false, false};
        bool m_ShowsHeader = true;
        bool m_HasArrangement = false;
        float m_Widths[3] = {0.0f, 0.0f, 0.0f};
        int m_Order[3] = {0, 1, 2};
        bool m_IsSorting = false;
        TableSort m_Sort;
        int m_SortChanges = 0;
        bool m_WidthsChanged = false;
        bool m_OrderChanged = false;
        std::string_view m_SizeText = "42 KB";
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

    TEST_F(TableTests, DraggingADividerResizesTheColumnBeforeIt)
    {
        m_HasArrangement = true;
        Settle(Interface());
        // The divider after the fixed column of 100 points, in the 24-point header.
        Drag(Vec2(5.0f + 100.0f, 12.0f), Vec2(5.0f + 140.0f, 12.0f), Interface());
        Settle(Interface());
        EXPECT_TRUE(m_WidthsChanged);
        EXPECT_FLOAT_EQ(m_Widths[0], 140.0f);
        EXPECT_FLOAT_EQ(m_Widths[1], 0.0f) << "the Fill columns keep sharing what is left";
        EXPECT_NEAR(m_Cells[0][1].X, 5.0f + 140.0f + 8.0f, 0.5f);
        EXPECT_FALSE(m_Done[0]) << "the drag does not reach the cells";

        // A column does not get narrower than its minimum width.
        Drag(Vec2(5.0f + 140.0f, 12.0f), Vec2(0.0f, 12.0f), Interface());
        Settle(Interface());
        EXPECT_FLOAT_EQ(m_Widths[0], 40.0f);
        EXPECT_NEAR(m_Cells[0][1].X, 5.0f + 40.0f + 8.0f, 0.5f);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(TableTests, ResizedWidthsAreRememberedWithoutStorage)
    {
        Settle(Interface());
        Drag(Vec2(5.0f + 100.0f, 12.0f), Vec2(5.0f + 160.0f, 12.0f), Interface());
        Settle(Interface());
        EXPECT_NEAR(m_Cells[0][1].X, 5.0f + 160.0f + 8.0f, 0.5f);
    }

    TEST_F(TableTests, DoubleClickingADividerFitsTheColumn)
    {
        m_HasArrangement = true;
        m_SizeText = "A rather long size description";
        Settle(Interface());
        // The divider after the last column, at the trailing edge of the rows.
        const Vec2 divider(400.0f - 5.0f, 12.0f);
        Click(divider, Interface());
        Click(divider, Interface());
        Settle(Interface());
        EXPECT_TRUE(m_WidthsChanged);
        TextSpec spec = GetTextSpec(TextStyle::Body);
        spec.Wraps = false;
        EXPECT_NEAR(m_Widths[2], MeasureText(m_SizeText, spec).X + 16.0f, 0.5f);
    }

    TEST_F(TableTests, TheApplicationSetsWidthsAndOrder)
    {
        m_HasArrangement = true;
        m_Widths[1] = 120.0f;
        m_Order[0] = 2;
        m_Order[1] = 0;
        m_Order[2] = 1;
        Settle(Interface());
        // Size, the only flexible column, comes first and takes what the others leave of 390 points.
        EXPECT_NEAR(m_Cells[0][0].X, 5.0f + 170.0f + 8.0f, 0.5f);
        EXPECT_NEAR(m_Cells[0][1].X, 5.0f + 170.0f + 100.0f + 8.0f, 0.5f);
        EXPECT_FALSE(m_OrderChanged) << "only changes made by the user are reported";

        // An order that is no permutation is reset, also in the application's storage.
        m_Order[0] = 1;
        m_Order[1] = 1;
        m_Order[2] = 0;
        Frame(Interface());
        EXPECT_EQ(m_Order[0], 0);
        EXPECT_EQ(m_Order[1], 1);
        EXPECT_EQ(m_Order[2], 2);
    }

    TEST_F(TableTests, ClickingAHeaderSortsByItsColumn)
    {
        m_IsSorting = true;
        Settle(Interface());
        // Name: from 105 to 105 + 96.7; Size after it.
        Click(Vec2(150.0f, 12.0f), Interface());
        EXPECT_EQ(m_Sort, (TableSort{.Column = 1, .Direction = SortDirection::Ascending}));
        EXPECT_EQ(m_SortChanges, 1);
        Click(Vec2(150.0f, 12.0f), Interface());
        EXPECT_EQ(m_Sort, (TableSort{.Column = 1, .Direction = SortDirection::Descending}));
        EXPECT_EQ(m_SortChanges, 2);

        // A column starts in its own direction.
        Click(Vec2(300.0f, 12.0f), Interface());
        EXPECT_EQ(m_Sort, (TableSort{.Column = 2, .Direction = SortDirection::Descending}));

        // A column that does not sort ignores the click, and so does a press that ends elsewhere.
        Click(Vec2(50.0f, 12.0f), Interface());
        EXPECT_EQ(m_Sort.Column, 2);
        Drag(Vec2(150.0f, 12.0f), Vec2(150.0f, 120.0f), Interface());
        EXPECT_EQ(m_Sort.Column, 2);
        EXPECT_EQ(m_SortChanges, 3);
        EXPECT_EQ(m_Selected, -1) << "clicks on the header do not reach the rows";
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(TableTests, AHeaderWithoutSortIsNotInteractive)
    {
        Settle(Interface());
        Click(Vec2(150.0f, 12.0f), Interface());
        EXPECT_EQ(m_SortChanges, 0);
        // Without sorting, the header is no stop for Tab: the rows are the first.
        TapKey(Key::Tab, Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 0);
    }

    TEST_F(TableTests, TheKeyboardSortsFromTheHeader)
    {
        m_IsSorting = true;
        Settle(Interface());
        // The header is the first stop for Tab; the left and right arrows move between its columns.
        TapKey(Key::Tab, Interface());
        TapKey(Key::RightArrow, Interface());
        TapKey(Key::Space, Interface());
        EXPECT_EQ(m_Sort, (TableSort{.Column = 1, .Direction = SortDirection::Ascending}));
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Sort, (TableSort{.Column = 1, .Direction = SortDirection::Descending}));
        TapKey(Key::RightArrow, Interface());
        TapKey(Key::RightArrow, Interface());
        TapKey(Key::Space, Interface());
        EXPECT_EQ(m_Sort.Column, 2);
        EXPECT_EQ(m_Selected, -1) << "the rows did not have focus";

        // The next stop is the rows.
        TapKey(Key::Tab, Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Selected, 0);
    }

    TEST_F(TableTests, DraggingAHeaderMovesTheColumn)
    {
        m_HasArrangement = true;
        m_IsSorting = true;
        Settle(Interface());
        // Done (100 points) is dragged by its header until its middle passes the middle of Name (96.7 points).
        MoveMouse(Vec2(50.0f, 12.0f), Interface());
        PressMouse(Interface());
        MoveMouse(Vec2(180.0f, 12.0f), Interface());
        // Name makes room: it slides towards the leading edge rather than jumping there.
        EXPECT_GT(m_Cells[0][1].X, 5.0f + 8.0f + 1.0f);
        EXPECT_LT(m_Cells[0][1].X, 5.0f + 100.0f + 8.0f - 1.0f);
        // Done follows the pointer, in the cells too.
        EXPECT_NEAR(m_Cells[0][0].X, 5.0f + 130.0f + 8.0f, 0.5f);
        EXPECT_FALSE(m_OrderChanged) << "the order is reported when the column is dropped";
        ReleaseMouse(Interface());
        EXPECT_TRUE(m_OrderChanged);
        EXPECT_EQ(m_Order[0], 1);
        EXPECT_EQ(m_Order[1], 0);
        EXPECT_EQ(m_Order[2], 2);
        Settle(Interface(), 60);
        EXPECT_NEAR(m_Cells[0][1].X, 5.0f + 8.0f, 0.5f);
        EXPECT_NEAR(m_Cells[0][0].X, 5.0f + 96.7f + 8.0f, 0.5f);
        EXPECT_EQ(m_SortChanges, 0) << "a drag is no click";
        EXPECT_FALSE(m_Done[0]);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(TableTests, MovedColumnsAreRememberedWithoutStorage)
    {
        Settle(Interface());
        // Size (193.3 points) is dragged to the leading edge, where it stops; its middle passes the middle of
        // Done but not that of Name.
        Drag(Vec2(300.0f, 12.0f), Vec2(20.0f, 12.0f), Interface());
        Settle(Interface(), 60);
        EXPECT_NEAR(m_Cells[0][0].X, 5.0f + 8.0f, 0.5f);
        EXPECT_NEAR(m_Cells[0][1].X, 5.0f + 100.0f + 193.3f + 8.0f, 0.5f);
    }

    TEST_F(TableTests, TheKeyboardMovesAColumn)
    {
        m_HasArrangement = true;
        m_IsSorting = true;
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        TapKey(Key::LeftCtrl, Key::RightArrow, Interface());
        EXPECT_TRUE(m_OrderChanged);
        EXPECT_EQ(m_Order[0], 1);
        EXPECT_EQ(m_Order[1], 0);
        // The header's focus moved with Done; Name is to its left now.
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::Space, Interface());
        EXPECT_EQ(m_Sort.Column, 1);
        EXPECT_EQ(m_SortChanges, 1);
    }

    TEST_F(TableTests, ManyColumnsScrollSideways)
    {
        constexpr int ColumnCount = 48;
        TableColumn columns[ColumnCount];
        for (TableColumn& column : columns)
            column = {.Title = "Column", .Width = 100.0f};
        Rect first;
        Rect last;
        const Builder build = [&]
        {
            BeginTable("wide", columns, {.Width = 400.0f, .Height = 200.0f});
            for (int row = 0; row < 3; row++)
            {
                TableRow(row);
                for (int column = 0; column < ColumnCount; column++)
                {
                    BeginTableCell();
                    const Rect cell = AllocateItem(Vec2(10.0f, 10.0f));
                    if (row == 0 && column == 0)
                        first = cell;
                    if (row == 0 && column == ColumnCount - 1)
                        last = cell;
                    EndTableCell();
                }
            }
            EndTable();
        };
        Settle(build);
        EXPECT_FLOAT_EQ(first.X, 5.0f + 8.0f);
        EXPECT_FLOAT_EQ(last.X, 5.0f + 4700.0f + 8.0f);

        // A sideways wheel over the table scrolls it, 48 points per notch.
        MoveMouse(Vec2(200.0f, 100.0f), build);
        GetIO().AddMouseWheelEvent(-2.0f, 0.0f);
        Settle(build, 60);
        EXPECT_NEAR(first.X, 5.0f + 8.0f - 96.0f, 0.5f);

        // Shift with a plain wheel scrolls sideways too, as far as the last column.
        GetIO().AddKeyEvent(Key::LeftShift, true);
        GetIO().AddMouseWheelEvent(0.0f, -1000.0f);
        Frame(build);
        GetIO().AddKeyEvent(Key::LeftShift, false);
        Settle(build, 60);
        // The last column ends at the trailing edge of the rows.
        EXPECT_NEAR(last.X - 8.0f + 100.0f, 400.0f - 5.0f, 0.5f);
        EXPECT_TRUE(m_AssertMessages.empty());
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
