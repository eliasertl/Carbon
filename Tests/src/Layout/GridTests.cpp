#include "Support/WidgetTest.h"

namespace Carbon
{
    // Column widths are measured in one frame and used in the next, like a stack's alignment: tests that check
    // positions `Settle` first.
    class GridTests : public WidgetTest
    {
    protected:
        Rect Item(float width, float height, const ItemOptions& options = {})
        {
            return AllocateItem(Vec2(width, height), options);
        }
    };

    TEST_F(GridTests, ColumnsLineUpAcrossRowsOfDifferentWidths)
    {
        const Vec2 sizes[2][2] = {{Vec2(50.0f, 20.0f), Vec2(30.0f, 20.0f)}, {Vec2(80.0f, 10.0f), Vec2(40.0f, 10.0f)}};
        Rect cells[2][2], grid;
        Settle(
            [&]
            {
                BeginGrid({.HorizontalSpacing = 10.0f, .VerticalSpacing = 4.0f});
                for (int row = 0; row < 2; row++)
                {
                    BeginGridRow();
                    for (int column = 0; column < 2; column++)
                        cells[row][column] = Item(sizes[row][column].X, sizes[row][column].Y);
                    EndGridRow();
                }
                EndGrid();
                grid = GetLastItemRect();
            });
        // The first column is as wide as its widest cell (80), so the second starts at 90 in both rows.
        EXPECT_EQ(cells[0][0], Rect(0.0f, 0.0f, 50.0f, 20.0f));
        EXPECT_EQ(cells[0][1], Rect(90.0f, 0.0f, 30.0f, 20.0f));
        EXPECT_EQ(cells[1][0], Rect(0.0f, 24.0f, 80.0f, 10.0f));
        EXPECT_EQ(cells[1][1], Rect(90.0f, 24.0f, 40.0f, 10.0f));
        EXPECT_EQ(grid, Rect(0.0f, 0.0f, 130.0f, 34.0f));
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(GridTests, ACellCanSpanColumns)
    {
        float spanWidth = 60.0f;
        Rect second, spanning;
        const Builder build = [&]
        {
            BeginGrid({.HorizontalSpacing = 10.0f});
            BeginGridRow();
            Item(50.0f, 10.0f);
            second = Item(30.0f, 10.0f);
            EndGridRow();
            BeginGridRow();
            SetNextGridCell({.ColumnSpan = 2});
            spanning = Item(spanWidth, 10.0f);
            EndGridRow();
            EndGrid();
        };

        // A span narrower than its columns leaves them alone.
        Settle(build);
        EXPECT_EQ(second.X, 60.0f);
        EXPECT_EQ(spanning.X, 0.0f);

        // A wider one grows them evenly: 200 - (50 + 10 + 30) = 110 more, 55 each.
        spanWidth = 200.0f;
        Settle(build);
        EXPECT_EQ(second.X, 115.0f);
        EXPECT_EQ(spanning, Rect(0.0f, 18.0f, 200.0f, 10.0f));
    }

    TEST_F(GridTests, AlignmentComesFromTheCellThenTheColumnThenTheGrid)
    {
        static constexpr Alignment ColumnAlignments[] = {Alignment::Leading, Alignment::Center};
        Rect leading, centered, overridden, trailing;
        Settle(
            [&]
            {
                BeginGrid({.HorizontalSpacing = 10.0f,
                           .Alignment = Alignment::Trailing,
                           .ColumnAlignments = ColumnAlignments});
                BeginGridRow();
                Item(80.0f, 10.0f);
                Item(40.0f, 10.0f);
                Item(40.0f, 10.0f);
                EndGridRow();
                BeginGridRow();
                leading = Item(50.0f, 10.0f);
                centered = Item(20.0f, 10.0f);
                trailing = Item(10.0f, 10.0f);
                EndGridRow();
                BeginGridRow();
                Item(50.0f, 10.0f);
                SetNextGridCell({.Alignment = Alignment::Trailing});
                overridden = Item(20.0f, 10.0f);
                EndGridRow();
                EndGrid();
            });
        EXPECT_EQ(leading.X, 0.0f);      // column 0: Leading
        EXPECT_EQ(centered.X, 100.0f);   // column 1 (90..130): Center
        EXPECT_EQ(overridden.X, 110.0f); // the cell's own Trailing
        EXPECT_EQ(trailing.X, 170.0f);   // column 2 (140..180): the grid's Trailing
    }

    TEST_F(GridTests, VerticalAlignmentComesFromTheCellThenTheRowThenTheGrid)
    {
        Rect centered, top, bottom;
        Settle(
            [&]
            {
                BeginGrid({.VerticalSpacing = 0.0f});
                BeginGridRow();
                Item(10.0f, 30.0f);
                centered = Item(10.0f, 10.0f);
                SetNextGridCell({.VerticalAlignment = VerticalAlignment::Bottom});
                bottom = Item(10.0f, 10.0f);
                EndGridRow();
                BeginGridRow({.Alignment = VerticalAlignment::Top});
                Item(10.0f, 30.0f);
                top = Item(10.0f, 10.0f);
                EndGridRow();
                EndGrid();
            });
        EXPECT_EQ(centered.Y, 10.0f);
        EXPECT_EQ(bottom.Y, 20.0f);
        EXPECT_EQ(top.Y, 30.0f);
    }

    TEST_F(GridTests, CellsCanHoldStacks)
    {
        Rect upper, lower, stack, beside, rowStackFirst, rowStackSecond;
        Settle(
            [&]
            {
                BeginGrid(
                    {.HorizontalSpacing = 10.0f, .VerticalSpacing = 0.0f, .VerticalAlignment = VerticalAlignment::Top});
                BeginGridRow();
                BeginVStack({.Spacing = 2.0f});
                upper = Item(60.0f, 10.0f);
                lower = Item(40.0f, 10.0f);
                EndVStack();
                stack = GetLastItemRect();
                beside = Item(20.0f, 20.0f);
                EndGridRow();
                BeginGridRow();
                Item(100.0f, 10.0f);
                BeginHStack({.Spacing = 4.0f});
                rowStackFirst = Item(10.0f, 10.0f);
                rowStackSecond = Item(10.0f, 10.0f);
                EndHStack();
                EndGridRow();
                EndGrid();
            });
        EXPECT_EQ(stack, Rect(0.0f, 0.0f, 60.0f, 22.0f));
        EXPECT_EQ(upper, Rect(0.0f, 0.0f, 60.0f, 10.0f));
        EXPECT_EQ(lower, Rect(0.0f, 12.0f, 40.0f, 10.0f));
        // The first column is 100 wide because of the second row.
        EXPECT_EQ(beside, Rect(110.0f, 0.0f, 20.0f, 20.0f));
        EXPECT_EQ(rowStackFirst, Rect(110.0f, 22.0f, 10.0f, 10.0f));
        EXPECT_EQ(rowStackSecond, Rect(124.0f, 22.0f, 10.0f, 10.0f));
    }

    TEST_F(GridTests, FillStretchesToTheColumnWhichMeasuresTheContent)
    {
        Rect filled, fullWidth, wideFill, narrowFill;
        Settle(
            [&]
            {
                BeginGrid({.HorizontalSpacing = 10.0f});
                BeginGridRow();
                Item(80.0f, 10.0f);
                Item(20.0f, 10.0f);
                EndGridRow();
                BeginGridRow();
                filled = Item(10.0f, 10.0f, {.Width = Size::Fill()});
                EndGridRow();
                // Directly in the grid, not in a row: spans the grid.
                fullWidth = Item(0.0f, 1.0f, {.Width = Size::Fill()});
                EndGrid();
            });
        EXPECT_EQ(filled.Width, 80.0f);
        EXPECT_EQ(fullWidth.Width, 110.0f);

        // A column of Fill cells only is as wide as the widest content, and every cell takes that width.
        Settle(
            [&]
            {
                BeginGrid();
                BeginGridRow();
                BeginHStack({.Padding = 5.0f, .Width = Size::Fill()});
                Item(30.0f, 10.0f);
                EndHStack();
                wideFill = GetLastItemRect();
                EndGridRow();
                BeginGridRow();
                narrowFill = Item(10.0f, 10.0f, {.Width = Size::Fill()});
                EndGridRow();
                EndGrid();
            });
        EXPECT_EQ(wideFill.Width, 40.0f);
        EXPECT_EQ(narrowFill.Width, 40.0f);
    }

    TEST_F(GridTests, GridsNestInCells)
    {
        Rect inner, after;
        Settle(
            [&]
            {
                BeginGrid({.HorizontalSpacing = 10.0f});
                BeginGridRow();
                BeginGrid({.HorizontalSpacing = 2.0f});
                BeginGridRow();
                Item(20.0f, 10.0f);
                inner = Item(20.0f, 10.0f);
                EndGridRow();
                EndGrid();
                after = Item(10.0f, 10.0f);
                EndGridRow();
                EndGrid();
            });
        EXPECT_EQ(inner.X, 22.0f);
        EXPECT_EQ(after.X, 52.0f);
    }

    TEST_F(GridTests, WrappingWidgetsInAGridKeepsTheirIDsAndFocus)
    {
        bool inGrid = false;
        bool inStack = false;
        ID buttonID;
        const Builder build = [&]
        {
            if (inStack)
                BeginVStack();
            if (inGrid)
            {
                BeginGrid();
                BeginGridRow();
                Text("Label");
            }
            Button("OK");
            buttonID = GetItemID();
            if (inGrid)
            {
                EndGridRow();
                EndGrid();
            }
            if (inStack)
                EndVStack();
        };

        Settle(build);
        const ID plainID = buttonID;
        TapKey(Key::Tab, build);
        ASSERT_EQ(GetFocusedID(), plainID);

        inGrid = true;
        Settle(build, 3);
        EXPECT_EQ(buttonID, plainID);
        EXPECT_EQ(GetFocusedID(), plainID);

        inStack = true;
        Settle(build, 3);
        EXPECT_EQ(buttonID, plainID);
        EXPECT_EQ(GetFocusedID(), plainID);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(GridTests, ANewGridSettlesAndStopsAnimating)
    {
        const Builder build = [&]
        {
            BeginGrid();
            BeginGridRow();
            Item(40.0f, 10.0f);
            EndGridRow();
            EndGrid();
        };
        Frame(build);
        EXPECT_TRUE(IsAnimating());
        Settle(build);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(GridTests, MisuseIsReported)
    {
        Frame([&] { BeginGrid(); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("Unbalanced layout"), std::string::npos);

        m_AssertMessages.clear();
        Frame(
            [&]
            {
                BeginGridRow();
                EndGridRow();
            });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("directly inside BeginGrid"), std::string::npos);

        m_AssertMessages.clear();
        Frame([&] { SetNextGridCell({.ColumnSpan = 2}); });
        ASSERT_EQ(m_AssertMessages.size(), 1u);
        EXPECT_NE(m_AssertMessages[0].find("inside a grid row"), std::string::npos);

        m_AssertMessages.clear();
        Frame(
            [&]
            {
                BeginGrid();
                BeginGridRow();
                EndGrid();
            });
        EXPECT_FALSE(m_AssertMessages.empty());

        // The frame after a mistake is clean again.
        m_AssertMessages.clear();
        Frame(
            [&]
            {
                BeginGrid();
                BeginGridRow();
                EndGridRow();
                EndGrid();
            });
        EXPECT_TRUE(m_AssertMessages.empty());
    }
} // namespace Carbon
