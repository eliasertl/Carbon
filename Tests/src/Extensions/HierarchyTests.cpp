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

    // ---- PathControl --------------------------------------------------------------------------------------------

    class PathControlTests : public WidgetTest
    {
    protected:
        static constexpr float FieldInset = 3.0f;
        static constexpr float Padding = 5.0f;
        static constexpr float IconSize = 16.0f;
        static constexpr float IconGap = 4.0f;
        static constexpr float Chevron = 12.0f;
        static constexpr float Collapsed = Padding * 2.0f + IconSize;

        Builder Interface()
        {
            return [this]
            {
                // Away from the top edge, so that the pop-up style's menu can open over the button.
                BeginVStack({.Padding = EdgeInsets(20.0f, 100.0f)});
                const int activated =
                    PathControl("path", std::span<const PathControlItem>(m_Path.data(), m_Path.size()), m_Options);
                m_Rect = GetItemRect();
                m_Id = GetItemID();
                EndVStack();
                if (activated >= 0)
                {
                    m_Activated = activated;
                    m_Activations++;
                }
            };
        }

        float GetFullWidth(size_t index) const
        {
            const PathControlItem& item = m_Path[index];
            const float label = MeasureText(item.Label, GetTextSpec(TextStyle::Body)).X;
            return Padding * 2.0f + (item.Icon.empty() ? 0.0f : IconSize + IconGap) + label;
        }

        // The left edge of a component, given the widths of the ones before it.
        float GetStart(std::initializer_list<float> widthsBefore) const
        {
            float x = m_Rect.X + FieldInset;
            for (const float width : widthsBefore)
                x += width + Chevron;
            return x;
        }

        // Just wide enough for the first and last name with the two middle components as icons.
        float GetNarrowWidth() const
        {
            return FieldInset * 2.0f + Chevron * 3.0f + GetFullWidth(0) + Collapsed * 2.0f + GetFullWidth(3);
        }

        void MoveAway()
        {
            MoveMouse(Vec2(0.0f, 500.0f), Interface());
            Settle(Interface());
        }

        std::vector<PathControlItem> m_Path = {
            {.Label = "Macintosh HD", .Icon = Icons::HardDrives},
            {.Label = "Documents", .Icon = Icons::Folder},
            {.Label = "Projects", .Icon = Icons::Folder},
            {.Label = "Report.pdf", .Icon = Icons::FilePdf},
        };
        PathControlOptions m_Options;
        int m_Activated = -1;
        int m_Activations = 0;
        Rect m_Rect;
        ID m_Id;
    };

    TEST_F(PathControlTests, ClickingAComponentReportsItsIndex)
    {
        Settle(Interface());
        for (size_t i = 0; i < m_Path.size(); i++)
        {
            float x = m_Rect.X + FieldInset;
            for (size_t j = 0; j < i; j++)
                x += GetFullWidth(j) + Chevron;
            Click(Vec2(x + GetFullWidth(i) * 0.5f, m_Rect.GetCenter().Y), Interface());
            EXPECT_EQ(m_Activated, int(i));
        }
        EXPECT_EQ(m_Activations, 4);
        // A click on a chevron activates nothing.
        Click(Vec2(GetStart({GetFullWidth(0)}) - Chevron * 0.5f, m_Rect.GetCenter().Y), Interface());
        EXPECT_EQ(m_Activations, 4);
    }

    TEST_F(PathControlTests, KeyboardMovesAHighlightAndActivatesIt)
    {
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        ASSERT_EQ(GetFocusedID(), m_Id);
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Activated, 3) << "the highlight starts at the selected item";
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::Space, Interface());
        EXPECT_EQ(m_Activated, 1);
        TapKey(Key::Home, Interface());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Activated, 0);
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Activated, 0) << "the highlight stops at the root";
        TapKey(Key::End, Interface());
        TapKey(Key::Enter, Interface());
        EXPECT_EQ(m_Activated, 3);
        EXPECT_EQ(m_Activations, 5);
    }

    TEST_F(PathControlTests, WhenNarrowTheMiddleNamesGiveWayFirst)
    {
        Settle(Interface());
        m_Options.Width = GetNarrowWidth();
        Settle(Interface());

        const float first = GetFullWidth(0);
        Click(Vec2(GetStart({first}) + Collapsed * 0.5f, m_Rect.GetCenter().Y), Interface());
        EXPECT_EQ(m_Activated, 1);
        MoveAway();
        Click(Vec2(GetStart({first, Collapsed}) + Collapsed * 0.5f, m_Rect.GetCenter().Y), Interface());
        EXPECT_EQ(m_Activated, 2);
        MoveAway();
        // The last name keeps its full width: the far end of the field is part of it.
        Click(Vec2(m_Rect.GetRight() - FieldInset - Padding, m_Rect.GetCenter().Y), Interface());
        EXPECT_EQ(m_Activated, 3);
    }

    TEST_F(PathControlTests, AHoveredComponentShowsItsName)
    {
        // Room for one of the two middle names. "Documents" is the longer one, so it gives way first.
        Settle(Interface());
        const float first = GetFullWidth(0);
        m_Options.Width = GetNarrowWidth() + GetFullWidth(1) - Collapsed + 1.0f;
        Settle(Interface());
        Click(Vec2(GetStart({first, Collapsed}) + GetFullWidth(2) - Padding, m_Rect.GetCenter().Y), Interface());
        EXPECT_EQ(m_Activated, 2);
        MoveAway();

        // Under the pointer, "Documents" shows its name and "Projects" makes room for it.
        MoveMouse(Vec2(GetStart({first}) + 4.0f, m_Rect.GetCenter().Y), Interface());
        Settle(Interface());
        Click(Vec2(GetStart({first}) + GetFullWidth(1) - Padding, m_Rect.GetCenter().Y), Interface());
        EXPECT_EQ(m_Activated, 1);
    }

    TEST_F(PathControlTests, VeryNarrowKeepsTheEndsReachable)
    {
        // Room for every component as an icon only.
        m_Options.Width = FieldInset * 2.0f + Chevron * 3.0f + Collapsed * 4.0f;
        Settle(Interface());
        Click(Vec2(m_Rect.X + FieldInset + 4.0f, m_Rect.GetCenter().Y), Interface());
        EXPECT_EQ(m_Activated, 0);
        MoveAway();
        Click(Vec2(m_Rect.GetRight() - FieldInset - 4.0f, m_Rect.GetCenter().Y), Interface());
        EXPECT_EQ(m_Activated, 3);
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(PathControlTests, PopUpStyleListsThePathInAMenu)
    {
        m_Options.Style = PathControlStyle::PopUp;
        Settle(Interface());
        const ID menu = HashID("##menu", m_Id);
        Click(m_Rect.GetCenter(), Interface());
        EXPECT_TRUE(IsOverlayOpen(menu));
        Settle(Interface());
        // The selected item lies over the button; its parents follow below it, the root last.
        Click(Vec2(m_Rect.X + 40.0f, m_Rect.GetCenter().Y + 22.0f * 2.0f), Interface());
        EXPECT_EQ(m_Activated, 1);
        EXPECT_FALSE(IsOverlayOpen(menu));

        // The keyboard opens it too.
        TapKey(Key::Tab, Interface());
        ASSERT_EQ(GetFocusedID(), m_Id);
        TapKey(Key::DownArrow, Interface());
        EXPECT_TRUE(IsOverlayOpen(menu));
    }

    TEST_F(PathControlTests, DisabledAndEmptyPathsDoNothing)
    {
        m_Options.Disabled = true;
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        EXPECT_EQ(m_Activations, 0);
        TapKey(Key::Tab, Interface());
        EXPECT_FALSE(GetFocusedID().IsValid());

        m_Options.Disabled = false;
        m_Path.clear();
        Settle(Interface());
        Click(m_Rect.GetCenter(), Interface());
        EXPECT_EQ(m_Activations, 0);
        EXPECT_TRUE(m_AssertMessages.empty());
    }
} // namespace Carbon
