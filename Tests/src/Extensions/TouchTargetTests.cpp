#include <Carbon/Extensions/Extensions.h>

#include <string>
#include <vector>

#include "Carbon/Core/ContextInternal.h"
#include "Support/WidgetTest.h"

namespace Carbon
{
    // Every interactive component, core and extension, in touch mode: each area that reacts to the pointer must be
    // at least MinimumTouchTarget points in both directions (the HIG's 44 x 44).
    class TouchTargetTests : public WidgetTest
    {
    protected:
        void SetUp() override
        {
            WidgetTest::SetUp();
            GetIO().SetDisplaySize(1400.0f, 2400.0f);
            GetIO().SetTouchModeOverride(true);
        }

        void BuildEverything()
        {
            BeginVStack({.Padding = 40.0f});

            // Core components.
            Button("Button");
            Button("Small", {.ControlSize = ControlSize::Small});
            Button("##icon", {.Role = ButtonRole::Plain, .Icon = Icons::Plus});
            Toggle("Switch", &m_Flag);
            Toggle("Checkbox", &m_Flag, {.Kind = ToggleKind::Checkbox});
            Slider("Slider", &m_Value, 0.0f, 1.0f, {.Width = 200.0f});
            TextField("Field", &m_Text, {.Width = 200.0f});
            TextArea("Area", &m_Text, {.Width = 200.0f, .Height = 80.0f});
            BeginScrollView("scroll", {.Width = 200.0f, .Height = 60.0f});
            AllocateItem(Vec2(100.0f, 400.0f));
            EndScrollView();

            // Extensions.
            static const std::string_view Items[] = {"One", "Two", "Three"};
            SegmentedControl("Segments", &m_Selected, Items);
            RadioGroup("Radio", &m_Selected, Items);
            PopUpButton("PopUp", &m_Selected, Items);
            if (BeginPullDownButton("PullDown"))
            {
                MenuItem("Item");
                EndPullDownButton();
            }
            ComboBox("Combo", &m_Text, Items);
            TokenField("Tokens", &m_Tokens);
            Stepper("Stepper", &m_Number);
            NumberField("Number", &m_Number);
            ScrubField("Scrub", &m_Number);
            SearchField("Search", &m_Text);
            ColorWell("Color", &m_Color);
            DatePicker("Date", &m_Date);
            DatePickerCalendar("Calendar", &m_Date);
            static const PathControlItem Path[] = {{.Label = "Disk"}, {.Label = "Users"}, {.Label = "Report"}};
            PathControl("Path", Path);

            BeginTabView("tabs", &m_Selected, Items, {.Height = Size::Fixed(80.0f)});
            EndTabView();
            BeginToolbar("toolbar");
            ToolbarItem("Share", {.Icon = Icons::Export});
            EndToolbar();
            BeginMenuBar();
            if (BeginMenuBarMenu("File"))
                EndMenuBarMenu();
            EndMenuBar();

            BeginSidebar("sidebar", {.Width = 200.0f});
            SidebarItem("Inbox", true);
            EndSidebar();
            BeginList("list", {.Height = 120.0f});
            ListItem("A", false);
            ListItem("B", true);
            EndList();
            static const TableColumn Columns[] = {{.Title = "Name"}, {.Title = "Size"}};
            BeginTable("table", Columns, {.Height = 120.0f});
            TableRow("row");
            TableCell("a");
            TableCell("b");
            EndTable();
            BeginOutlineView("outline", {.Height = 120.0f});
            if (BeginOutlineItem("Folder", false, {.HasChildren = true}).IsExpanded)
            {
            }
            EndOutlineItem();
            EndOutlineView();
            BeginSplitView("split", {.Height = Size::Fixed(100.0f)});
            SplitViewDivider();
            EndSplitView();

            EndVStack();
        }

        bool m_Flag = false;
        float m_Value = 0.5f;
        std::string m_Text = "Text";
        int m_Selected = 0;
        double m_Number = 1.0;
        std::vector<std::string> m_Tokens = {"Red", "Green"};
        Color m_Color = Color(0.2f, 0.4f, 0.8f);
        DateTime m_Date = {.Year = 2026, .Month = 10, .Day = 10};
    };

    TEST_F(TouchTargetTests, EveryControlHasAFingerSizedHitArea)
    {
        Settle([&] { BuildEverything(); });
        m_Context->Interaction.IsRecordingHitRects = true;
        Frame([&] { BuildEverything(); });
        const std::vector<Internal::InteractionState::HitRect>& hits = m_Context->Interaction.HitRects;
        EXPECT_GT(hits.size(), 40u);
        for (const Internal::InteractionState::HitRect& hit : hits)
        {
            EXPECT_GE(hit.Area.Width, MinimumTouchTarget - 0.01f) << "item " << hit.Id.Value;
            EXPECT_GE(hit.Area.Height, MinimumTouchTarget - 0.01f) << "item " << hit.Id.Value;
        }
        m_Context->Interaction.IsRecordingHitRects = false;
    }

    TEST_F(TouchTargetTests, ATapNextToASmallControlReachesIt)
    {
        bool value = false;
        Rect box;
        const Builder build = [&]
        {
            SetCursorPos(Vec2(100.0f, 100.0f));
            Toggle("Checkbox", &value, {.Kind = ToggleKind::Checkbox});
            box = GetItemRect();
        };
        Settle(build);
        // Above the checkbox, outside what is drawn, but within 44 points around it.
        const Vec2 above(box.X + 6.0f, box.GetCenter().Y - MinimumTouchTarget * 0.5f + 2.0f);
        ASSERT_LT(above.Y, box.Y);
        Tap(above, build);
        EXPECT_TRUE(value);

        // With a mouse the same point misses it.
        GetIO().SetTouchModeOverride(false);
        Click(above, build);
        EXPECT_TRUE(value);
    }

    TEST_F(TouchTargetTests, OverlappingHitAreasGoToTheNearestControl)
    {
        int clicked = -1;
        const Builder build = [&]
        {
            // Two 20-point buttons 4 points apart: their 44-point areas overlap.
            for (int i = 0; i < 2; i++)
            {
                const Rect rect(100.0f, 100.0f + static_cast<float>(i) * 24.0f, 60.0f, 20.0f);
                if (ButtonBehavior(HashID(i, ID{1}), rect).Clicked)
                    clicked = i;
            }
        };
        Settle(build);
        Tap(Vec2(130.0f, 121.0f), build); // 1 point below the first, 3 above the second
        EXPECT_EQ(clicked, 0);
        Tap(Vec2(130.0f, 123.0f), build);
        EXPECT_EQ(clicked, 1);
    }

    TEST_F(TouchTargetTests, RowsAreFingerTallInTouchModeOnly)
    {
        Rect row;
        const Builder build = [&]
        {
            BeginList("list", {.Height = 300.0f});
            ListItem("A", false);
            row = GetItemRect();
            EndList();
        };
        Settle(build);
        EXPECT_GE(row.Height, MinimumTouchTarget);
        GetIO().SetTouchModeOverride(false);
        Settle(build);
        EXPECT_FLOAT_EQ(row.Height, 24.0f);
    }

    TEST_F(TouchTargetTests, ALongPressShowsTheTooltipWithoutActivating)
    {
        int clicks = 0;
        const Builder build = [&]
        {
            SetCursorPos(Vec2(100.0f, 100.0f));
            clicks += Button("Delete") ? 1 : 0;
            Tooltip("Removes the item");
        };
        Settle(build);
        const size_t quiet = GetDrawData().Vertices.size();
        TouchDown(Vec2(110.0f, 110.0f), build);
        for (int i = 0; i < 40; i++)
            Frame(build);
        EXPECT_GT(GetDrawData().Vertices.size(), quiet); // the tooltip is drawn
        TouchUp(Vec2(110.0f, 110.0f), build);
        EXPECT_EQ(clicks, 0);
        Settle(build, 200);
        EXPECT_EQ(GetDrawData().Vertices.size(), quiet); // and gone a moment after the finger was lifted
    }
} // namespace Carbon
