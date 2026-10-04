#include <Carbon/Extensions/Extensions.h>

#include <string>
#include <vector>

#include "Support/WidgetTest.h"

namespace Carbon
{
    // ---- OutlineView --------------------------------------------------------------------------------------------

    // A small tree:  A (A1 (A1a), A2), B. Without columns the rows start at (5, 5) and are 24 points high and
    // 790 points wide; each level is indented by 16 points.
    class OutlineViewTests : public WidgetTest
    {
    protected:
        struct Node
        {
            const char* Name;
            std::vector<Node> Children;
        };

        void BuildNode(const Node& node)
        {
            const OutlineItem item =
                BeginOutlineItem(node.Name, m_Selected == node.Name,
                                 {.HasChildren = !node.Children.empty(), .IsInitiallyExpanded = m_ExpandsInitially});
            m_Shown.push_back(node.Name);
            if (item.Picked)
                m_Selected = node.Name;
            if (item.Activated)
                m_Activated = node.Name;
            if (item.IsExpanded)
            {
                for (const Node& child : node.Children)
                    BuildNode(child);
            }
            EndOutlineItem();
        }

        Builder Interface()
        {
            return [this]
            {
                m_Shown.clear();
                BeginOutlineView("outline", {.Width = 800.0f, .Height = 400.0f});
                for (const Node& node : m_Tree)
                    BuildNode(node);
                EndOutlineView();
            };
        }

        // Center of the n-th row shown, and of its disclosure triangle at the given depth.
        static Vec2 GetRow(int row) { return Vec2(400.0f, 5.0f + 24.0f * static_cast<float>(row) + 12.0f); }
        static Vec2 GetDisclosure(int row, int depth)
        {
            return Vec2(5.0f + 16.0f * static_cast<float>(depth) + 9.0f, GetRow(row).Y);
        }

        std::vector<Node> m_Tree = {{"A", {{"A1", {{"A1a", {}}}}, {"A2", {}}}}, {"B", {}}};
        std::vector<std::string> m_Shown;
        std::string m_Selected;
        std::string m_Activated;
        bool m_ExpandsInitially = false;
    };

    TEST_F(OutlineViewTests, ItemsStartCollapsedUnlessAskedOtherwise)
    {
        Settle(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "B"}));

        m_ExpandsInitially = true;
        // The first appearance decides; items already seen keep their state.
        Settle(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "B"}));
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(OutlineViewTests, InitiallyExpandedItemsShowTheirChildren)
    {
        m_ExpandsInitially = true;
        Settle(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "A1", "A1a", "A2", "B"}));
    }

    TEST_F(OutlineViewTests, DisclosureTriangleExpandsAndCollapses)
    {
        Settle(Interface());
        Click(GetDisclosure(0, 0), Interface());
        Frame(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "A1", "A2", "B"}));
        EXPECT_TRUE(m_Selected.empty()) << "the triangle does not select the row";

        // A1 is one level deeper: its triangle is indented.
        Click(GetDisclosure(1, 1), Interface());
        Frame(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "A1", "A1a", "A2", "B"}));

        Click(GetDisclosure(0, 0), Interface());
        Frame(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "B"}));

        // Expanding A again shows A1 as the user left it: expanded.
        Click(GetDisclosure(0, 0), Interface());
        Frame(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "A1", "A1a", "A2", "B"}));
    }

    TEST_F(OutlineViewTests, ClickPicksAndDoubleClickActivates)
    {
        Settle(Interface());
        Click(GetRow(1), Interface());
        EXPECT_EQ(m_Selected, "B");
        EXPECT_TRUE(m_Activated.empty());
        Click(GetRow(1), Interface());
        EXPECT_EQ(m_Activated, "B");
    }

    TEST_F(OutlineViewTests, ArrowKeysWalkTheHierarchy)
    {
        Settle(Interface());
        Click(GetRow(0), Interface());
        ASSERT_EQ(m_Selected, "A");

        TapKey(Key::RightArrow, Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "A1", "A2", "B"})) << "right expands";
        TapKey(Key::RightArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Selected, "A1") << "right on an expanded item moves to its first child";
        TapKey(Key::DownArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Selected, "A2");
        TapKey(Key::LeftArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Selected, "A") << "left on a child moves to its parent";
        TapKey(Key::LeftArrow, Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "B"})) << "left collapses";

        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Activated, "A");
    }

    TEST_F(OutlineViewTests, AltExpandsEverythingInside)
    {
        Settle(Interface());
        Click(GetRow(0), Interface());
        TapKey(Key::LeftAlt, Key::RightArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "A1", "A1a", "A2", "B"}));

        // Alt-collapsing collapses the descendants too: expanding A again shows A1 collapsed.
        TapKey(Key::LeftAlt, Key::LeftArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "B"}));
        TapKey(Key::RightArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Shown, (std::vector<std::string>{"A", "A1", "A2", "B"}));
    }

    TEST_F(OutlineViewTests, ColumnsShowAttributes)
    {
        const TableColumn columns[] = {{.Title = "Name"}, {.Title = "Size", .Width = 100.0f}};
        int cells = 0;
        const Builder build = [&]
        {
            BeginOutlineView("outline", {.Width = 400.0f, .Height = 200.0f, .Columns = columns});
            BeginOutlineItem("Folder", false);
            OutlineCell("--");
            cells++;
            EndOutlineItem();
            EndOutlineView();
        };
        Settle(build);
        EXPECT_GT(cells, 0);
        EXPECT_TRUE(m_AssertMessages.empty());

        const Builder tooMany = [&]
        {
            BeginOutlineView("outline", {.Width = 400.0f, .Height = 200.0f, .Columns = columns});
            BeginOutlineItem("Folder", false);
            OutlineCell("--");
            OutlineCell("too many");
            EndOutlineItem();
            EndOutlineView();
        };
        Frame(tooMany);
        EXPECT_FALSE(m_AssertMessages.empty());
    }

    TEST_F(OutlineViewTests, MissingEndIsReported)
    {
        Frame(
            []
            {
                BeginOutlineView("outline");
                BeginOutlineItem("Unclosed", false);
                EndOutlineView();
            });
        ASSERT_FALSE(m_AssertMessages.empty());
        EXPECT_NE(m_AssertMessages[0].find("BeginOutlineItem"), std::string::npos);
        Frame(Interface());
        EXPECT_EQ(m_AssertMessages.size(), 1u) << "the next frame is clean again";
    }

    // ---- ColumnView ---------------------------------------------------------------------------------------------

    // Root: Folder (One, Two), File. Columns are 200 points wide plus a one-point divider; rows start 5 points
    // inside a column and are 24 points high.
    class ColumnViewTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                BeginColumnView("browser", {.Width = m_Width, .Height = 300.0f});
                BeginColumnViewColumn();
                static const char* const Root[] = {"Folder", "File"};
                for (int i = 0; i < 2; i++)
                {
                    if (ColumnViewItem(Root[i], !m_Path.empty() && m_Path[0] == i, {.HasChildren = i == 0}))
                        m_Path = {i};
                }
                EndColumnViewColumn();
                m_Columns = 1;
                if (!m_Path.empty() && m_Path[0] == 0)
                {
                    BeginColumnViewColumn();
                    static const char* const Children[] = {"One", "Two"};
                    for (int i = 0; i < 2; i++)
                    {
                        if (ColumnViewItem(Children[i], m_Path.size() > 1 && m_Path[1] == i))
                            m_Path = {0, i};
                    }
                    EndColumnViewColumn();
                    m_Columns = 2;
                }
                EndColumnView();
            };
        }

        static Vec2 GetItem(int column, int row)
        {
            return Vec2(201.0f * static_cast<float>(column) + 100.0f, 5.0f + 24.0f * static_cast<float>(row) + 12.0f);
        }

        std::vector<int> m_Path;
        int m_Columns = 0;
        float m_Width = 800.0f;
    };

    TEST_F(ColumnViewTests, PickingAFolderOpensTheNextColumn)
    {
        Settle(Interface());
        EXPECT_EQ(m_Columns, 1);
        Click(GetItem(0, 0), Interface());
        EXPECT_EQ(m_Path, (std::vector<int>{0}));
        Settle(Interface());
        EXPECT_EQ(m_Columns, 2);

        Click(GetItem(1, 1), Interface());
        EXPECT_EQ(m_Path, (std::vector<int>{0, 1}));

        // Picking in an earlier column shortens the path.
        Click(GetItem(0, 1), Interface());
        EXPECT_EQ(m_Path, (std::vector<int>{1}));
        Frame(Interface());
        EXPECT_EQ(m_Columns, 1);
    }

    TEST_F(ColumnViewTests, ArrowKeysMoveWithinAndBetweenColumns)
    {
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        TapKey(Key::DownArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Path, (std::vector<int>{0}));

        TapKey(Key::RightArrow, Interface());
        Settle(Interface(), 4);
        EXPECT_EQ(m_Path, (std::vector<int>{0, 0})) << "right moves into the next column and picks its first item";
        TapKey(Key::DownArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Path, (std::vector<int>{0, 1}));

        TapKey(Key::LeftArrow, Interface());
        Settle(Interface(), 4);
        EXPECT_EQ(m_Path, (std::vector<int>{0})) << "left moves back and picks the parent again";
        TapKey(Key::DownArrow, Interface());
        Frame(Interface());
        EXPECT_EQ(m_Path, (std::vector<int>{1})) << "the focus is in the first column";
    }

    TEST_F(ColumnViewTests, RightArrowOnAFileDoesNothing)
    {
        Settle(Interface());
        Click(GetItem(0, 1), Interface());
        TapKey(Key::RightArrow, Interface());
        Settle(Interface(), 4);
        EXPECT_EQ(m_Path, (std::vector<int>{1}));
    }

    TEST_F(ColumnViewTests, DividersResizeColumns)
    {
        m_Path = {0};
        Settle(Interface());
        // The divider after the first column is at x = 200.
        MoveMouse(Vec2(200.5f, 200.0f), Interface());
        PressMouse(Interface());
        GetIO().AddMousePosEvent(260.5f, 200.0f);
        Frame(Interface());
        ReleaseMouse(Interface());
        Settle(Interface());
        // The second column starts 60 points later now.
        Click(Vec2(261.0f + 100.0f, GetItem(1, 1).Y), Interface());
        EXPECT_EQ(m_Path, (std::vector<int>{0, 1}));
    }

    TEST_F(ColumnViewTests, NewColumnsAreScrolledIntoView)
    {
        m_Width = 300.0f;
        Settle(Interface());
        Click(GetItem(0, 0), Interface());
        Settle(Interface(), 60);
        float offset = 0.0f;
        Frame(
            [&]
            {
                Interface()();
                PushID("browser"); // the column view's scope
                offset = GetScrollOffset("##columns").X;
                PopID();
            });
        // Two columns of 201 points in a view of 300: scrolled to the end.
        EXPECT_NEAR(offset, 402.0f - 300.0f, 1.0f);
    }
} // namespace Carbon
