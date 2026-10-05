#include <atomic>
#include <cstdlib>
#include <new>

#include "Support/WidgetTest.h"

#if defined(CARBON_TESTS_HAVE_EXTENSIONS)
#include <Carbon/Extensions/Extensions.h>
#endif

// Counts every heap allocation of the test binary, so that a test can assert that a steady-state frame makes
// none. Replacing the global operators is the only portable way to see allocations made inside the library.
namespace
{
    std::atomic<size_t> g_AllocationCount{0};

    void* Allocate(std::size_t size)
    {
        g_AllocationCount.fetch_add(1, std::memory_order_relaxed);
        if (void* memory = std::malloc(size == 0 ? 1 : size))
            return memory;
        throw std::bad_alloc();
    }
} // namespace

void* operator new(std::size_t size)
{
    return Allocate(size);
}

void* operator new[](std::size_t size)
{
    return Allocate(size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace Carbon
{
    class AllocationTests : public WidgetTest
    {
    protected:
        // Builds the same interface for `warmUp` frames, then returns the number of allocations made by one
        // more frame.
        template <typename Build>
        size_t CountAllocationsOfOneFrame(const Build& build, int warmUp = 60)
        {
            for (int i = 0; i < warmUp; i++)
            {
                NewFrame();
                build();
                EndFrame();
            }
            const size_t before = g_AllocationCount.load();
            NewFrame();
            build();
            EndFrame();
            return g_AllocationCount.load() - before;
        }

        bool m_Toggle = true;
        float m_Slider = 0.4f;
        std::string m_Text = "Some text";
        int m_Selected = 1;
        DateTime m_Date = {.Year = 2026, .Month = 10, .Day = 5};
    };

    TEST_F(AllocationTests, CoreWidgetsDoNotAllocateInSteadyState)
    {
        const size_t allocations = CountAllocationsOfOneFrame(
            [this]
            {
                BeginScrollView("page", {.Spacing = 12.0f, .Padding = 20.0f});
                Text("Title", {.Style = TextStyle::Title1});
                Text("A longer paragraph that wraps onto several lines when the column is narrow enough for it.",
                     {.Secondary = true, .Width = 200.0f, .Wraps = true});
                BeginHStack({.Spacing = 8.0f, .Width = Size::Fill()});
                Button("Cancel");
                Spacer();
                Button("Save", {.Role = ButtonRole::Prominent, .Icon = Icons::Check});
                EndHStack();
                Toggle("Switch", &m_Toggle);
                Toggle("Checkbox", &m_Toggle, {.Kind = ToggleKind::Checkbox});
                Slider("Slider", &m_Slider, 0.0f, 1.0f);
                TextField("Field", &m_Text);
                Separator();
                Icon(Icons::Gear);
                Tooltip("Never shown, but evaluated");
                BeginGrid();
                for (int row = 0; row < 3; row++)
                {
                    BeginGridRow();
                    Text("Label");
                    SetNextGridCell({.ColumnSpan = row == 2 ? 2 : 1});
                    Button("Cell");
                    EndGridRow();
                }
                EndGrid();
                EndScrollView();
            });
        EXPECT_EQ(allocations, 0u);
    }

    TEST_F(AllocationTests, HoverFocusAndTypingDoNotAllocateInSteadyState)
    {
        const auto build = [this]
        {
            BeginVStack({.Spacing = 12.0f, .Padding = 20.0f});
            Button("Button");
            TextField("Field", &m_Text, {.Width = 200.0f});
            EndVStack();
        };
        // Hover the button, then focus the field and type: the first frames may grow buffers.
        GetIO().AddMousePosEvent(40.0f, 30.0f);
        CountAllocationsOfOneFrame(build, 5);
        GetIO().AddKeyEvent(Key::Tab, true);
        CountAllocationsOfOneFrame(build, 2);
        GetIO().AddKeyEvent(Key::Tab, false);
        GetIO().AddKeyEvent(Key::Tab, true);
        CountAllocationsOfOneFrame(build, 2);
        GetIO().AddKeyEvent(Key::Tab, false);
        m_Text.reserve(64);
        GetIO().AddInputCharactersUTF8("abc");
        CountAllocationsOfOneFrame(build, 5);

        GetIO().AddMousePosEvent(42.0f, 32.0f);
        EXPECT_EQ(CountAllocationsOfOneFrame(build, 30), 0u);
    }

#if defined(CARBON_TESTS_HAVE_EXTENSIONS)
    TEST_F(AllocationTests, ExtensionComponentsDoNotAllocateInSteadyState)
    {
        static const float Values[] = {1.0f, 4.0f, 2.0f, 8.0f, 5.0f};
        static const std::string_view Labels[] = {"A", "B", "C", "D", "E"};
        static const TableColumn Columns[] = {{.Title = "Name"}, {.Title = "Size", .Width = 80.0f}};
        OpenOverlay(GetID("popover"));
        OpenMenu("menu");
        PostNotification("Notification", {.Body = "Shown in every frame.",
                                          .Style = NotificationStyle::Info,
                                          .PrimaryAction = "Open",
                                          .Duration = 0.0f});

        const size_t allocations = CountAllocationsOfOneFrame(
            [this]
            {
                BeginHStack({.Spacing = 0.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                BeginSidebar("sidebar");
                SidebarHeader("Group");
                SidebarItem("First", m_Selected == 0, {.Icon = Icons::House});
                SidebarItem("Second", m_Selected == 1, {.Icon = Icons::Gear, .Badge = "3"});
                EndSidebar();

                BeginVStack({.Spacing = 10.0f, .Padding = 16.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                SegmentedControl("Segments", &m_Selected, {"One", "Two", "Three"});
                RadioGroup("Radios", &m_Selected, {"One", "Two", "Three"});
                static const PathControlItem Path[] = {{.Label = "Disk", .Icon = Icons::HardDrives},
                                                       {.Label = "Folder", .Icon = Icons::Folder},
                                                       {.Label = "File"}};
                PathControl("Path", Path, {.Width = 100.0f});
                PathControl("Pop-up path", Path, {.Style = PathControlStyle::PopUp});
                DatePickerCalendar("Calendar", &m_Date, {.Today = m_Date});
                BeginToolbar("Toolbar", {.Width = 200.0f});
                ToolbarItem("New", {.Icon = Icons::Plus});
                ToolbarSeparator();
                ToolbarFlexibleSpace();
                ToolbarItem("Share", {.Icon = Icons::Export});
                ToolbarItem("Delete", {.Icon = Icons::Trash});
                if (BeginToolbarControl("Find"))
                {
                    SearchField("Find", &m_Text, {.Width = 120.0f});
                    EndToolbarControl();
                }
                EndToolbar();
                PopUpButton("PopUp", &m_Selected, {"One", "Two", "Three"});
                Stepper("Stepper", &m_Selected);
                ProgressIndicator(0.5f);
                ProgressIndicator(0.0f, {.Kind = ProgressKind::Spinner, .IsIndeterminate = true});
                SearchField("Search", &m_Text);
                LineChart("line", Values, {.Height = 100.0f, .Labels = Labels});
                BarChart("bars", Values, {.Height = 100.0f, .Labels = Labels});
                BeginTable("table", Columns, {.Height = 100.0f});
                for (int i = 0; i < 3; i++)
                {
                    TableRow(i, i == m_Selected);
                    TableCell("Row");
                    TableCell("12 KB", {.Secondary = true});
                }
                EndTable();
                if (BeginPopover("popover", {.Anchor = Rect(300.0f, 40.0f, 40.0f, 20.0f)}))
                {
                    Text("Popover");
                    EndPopover();
                }
                if (BeginMenu("menu", {.Anchor = Rect(500.0f, 40.0f, 40.0f, 20.0f)}))
                {
                    MenuItem("Copy", {.Icon = Icons::Copy, .Shortcut = "Ctrl+C"});
                    MenuSeparator();
                    MenuItem("Checked", {.IsChecked = true});
                    EndMenu();
                }
                BeginMenuBar();
                if (BeginMenuBarMenu("File"))
                {
                    MenuItem("New");
                    EndMenuBarMenu();
                }
                EndMenuBar();
                ComboBox("Combo", &m_Text, Labels);
                BeginOutlineView("outline", {.Height = 100.0f, .Columns = Columns});
                const bool isExpanded = BeginOutlineItem("Folder", true, {.IsInitiallyExpanded = true}).IsExpanded;
                OutlineCell("--");
                if (isExpanded)
                {
                    BeginOutlineItem("File", false, {.HasChildren = false});
                    OutlineCell("1 KB");
                    EndOutlineItem();
                }
                EndOutlineItem();
                EndOutlineView();
                BeginColumnView("columns", {.Height = 100.0f});
                BeginColumnViewColumn();
                ColumnViewItem("Folder", true, {.HasChildren = true});
                EndColumnViewColumn();
                BeginColumnViewColumn();
                ColumnViewItem("File", true);
                EndColumnViewColumn();
                BeginColumnViewPreview();
                Text("Preview");
                EndColumnViewPreview();
                EndColumnView();
                EndVStack();
                EndHStack();
                ShowNotifications();
            },
            120);
        EXPECT_EQ(allocations, 0u);
        EXPECT_TRUE(m_AssertMessages.empty()) << "a misused component would allocate to report it";
    }
#endif
} // namespace Carbon
