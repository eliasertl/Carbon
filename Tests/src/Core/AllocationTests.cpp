#include <atomic>
#include <cstdlib>
#include <new>

#include "Support/WidgetTest.h"

#if defined(CARBON_TESTS_HAVE_EXTENSIONS)
#include <Carbon/Extensions/Extensions.h>
#endif
#if defined(CARBON_TESTS_HAVE_REFLECTION)
#include <Carbon/Reflection/Reflection.h>

namespace AllocationTestTypes
{
    enum class Quality
    {
        Low,
        High,
        VeryHigh
    };

    struct Audio
    {
        float Volume = 0.5f;
        int Bitrate = 128;
        bool Muted = false;
    };
    CB_REFLECT_STRUCT(Audio, CB_FIELD(Volume, {.Min = 0.0, .Max = 1.0, .Tooltip = "Output level"}),
                      CB_FIELD(Bitrate,
                               {.Min = 64, .Max = 320, .Step = 32, .Control = Carbon::ReflectControl::Stepper}),
                      CB_FIELD(Muted, {.Control = Carbon::ReflectControl::Checkbox}));

    struct Everything
    {
        Quality TextureQuality = Quality::High;
        bool VSync = true;
        double Gamma = 2.2;
        int Count = 3;
        std::string Name = "Settings";
        Carbon::Color Tint = Carbon::Color::FromHex(0x0A84FF);
        Carbon::DateTime Date = {.Year = 2026, .Month = 10, .Day = 5};
        Audio Sound = {};
        int Secret = 0;
    };
    CB_REFLECT_STRUCT(Everything, CB_FIELD(Gamma, {.ReadOnly = true}), CB_FIELD(Secret, {.Hidden = true}));
} // namespace AllocationTestTypes
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
#if defined(CARBON_TESTS_HAVE_EXTENSIONS)
        DateTime m_Date = {.Year = 2026, .Month = 10, .Day = 5};
        std::vector<std::string> m_Tokens = {"Ada", "Grace", "Alan"};
#endif
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
                    PushID(row);
                    BeginGridRow();
                    Text("Label");
                    SetNextGridCell({.ColumnSpan = row == 2 ? 2 : 1});
                    Button("Cell");
                    EndGridRow();
                    PopID();
                }
                EndGrid();
                EndScrollView();
            });
        EXPECT_EQ(allocations, 0u);
    }

#if defined(CARBON_TESTS_HAVE_REFLECTION)
    TEST_F(AllocationTests, ReflectDoesNotAllocateInSteadyState)
    {
        AllocationTestTypes::Everything everything;
        AllocationTestTypes::Quality quality = AllocationTestTypes::Quality::Low;
        const auto build = [&]
        {
            BeginVStack({.Spacing = 12.0f, .Padding = 20.0f});
            Reflect("everything", &everything);
            Reflect("above", &everything,
                    {.EnumStyle = ReflectEnumStyle::RadioGroup, .Layout = ReflectLayout::LabelAbove});
            Reflect("quality", &quality, {.EnumStyle = ReflectEnumStyle::SegmentedControl});
            EndVStack();
        };
        // Hover a row with a tooltip, then measure a settled frame.
        GetIO().AddMousePosEvent(40.0f, 40.0f);
        EXPECT_EQ(CountAllocationsOfOneFrame(build, 120), 0u);
        EXPECT_TRUE(m_AssertMessages.empty());
    }
#endif

#if defined(CARBON_TESTS_HAVE_EXTENSIONS)
    TEST_F(AllocationTests, EditingADateDoesNotAllocateInSteadyState)
    {
        const auto build = [this]
        {
            BeginVStack({.Padding = 20.0f});
            DatePicker("Date", &m_Date, {.Elements = DatePickerElements::DateAndTime, .Today = m_Date});
            EndVStack();
        };
        // Focus the field, open the calendar with Space and type into the elements.
        CountAllocationsOfOneFrame(build, 5);
        GetIO().AddKeyEvent(Key::Tab, true);
        CountAllocationsOfOneFrame(build, 2);
        GetIO().AddKeyEvent(Key::Tab, false);
        GetIO().AddKeyEvent(Key::Space, true);
        CountAllocationsOfOneFrame(build, 2);
        GetIO().AddKeyEvent(Key::Space, false);
        // Text that has never been shown is shaped once, which may allocate: type during the warm-up.
        GetIO().AddInputCharactersUTF8("2027-12");
        CountAllocationsOfOneFrame(build, 10);
        GetIO().AddKeyEvent(Key::UpArrow, true);
        CountAllocationsOfOneFrame(build, 10);
        GetIO().AddKeyEvent(Key::UpArrow, false);

        EXPECT_EQ(CountAllocationsOfOneFrame(build, 30), 0u);
        EXPECT_EQ(m_Date.Year, 2027);
        EXPECT_EQ(m_Date.Month, 12);
        EXPECT_EQ(m_Date.Day, 6);
    }

    TEST_F(AllocationTests, TypingIntoATokenFieldDoesNotAllocateInSteadyState)
    {
        const auto build = [this]
        {
            BeginVStack({.Padding = 20.0f});
            TokenField("Tokens", &m_Tokens);
            EndVStack();
        };
        // Focus the field, type some text and select a token. New tokens allocate; text that stays text and
        // selection do not, once the text has been shaped during the warm-up.
        CountAllocationsOfOneFrame(build, 5);
        GetIO().AddKeyEvent(Key::Tab, true);
        CountAllocationsOfOneFrame(build, 2);
        GetIO().AddKeyEvent(Key::Tab, false);
        GetIO().AddInputCharactersUTF8("Barbara");
        CountAllocationsOfOneFrame(build, 10);
        EXPECT_EQ(CountAllocationsOfOneFrame(build, 30), 0u);
        EXPECT_EQ(m_Tokens.size(), 3u);
    }
#endif

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

    TEST_F(AllocationTests, BufferAndCallbackTextFieldsDoNotAllocateInSteadyState)
    {
        char buffer[32] = "Fixed";
        std::string stored = "Stored";
        stored.reserve(64);
        const auto build = [&]
        {
            BeginVStack({.Spacing = 12.0f, .Padding = 20.0f});
            TextField("Buffer", buffer, {.Width = 200.0f});
            TextField("Callback", stored, [&](std::string_view text) { stored = text; }, {.Width = 200.0f});
            EndVStack();
        };
        // Neither field focused, then each focused in turn and typed into: the first frames may grow buffers.
        CountAllocationsOfOneFrame(build, 5);
        EXPECT_EQ(CountAllocationsOfOneFrame(build, 5), 0u);
        for (int field = 0; field < 2; field++)
        {
            GetIO().AddKeyEvent(Key::Tab, true);
            CountAllocationsOfOneFrame(build, 2);
            GetIO().AddKeyEvent(Key::Tab, false);
            GetIO().AddInputCharactersUTF8("abc");
            CountAllocationsOfOneFrame(build, 5);
            EXPECT_EQ(CountAllocationsOfOneFrame(build, 30), 0u);
        }
        EXPECT_STREQ(buffer, "abc") << "tabbing in selected everything, so typing replaced it";
        EXPECT_EQ(stored, "abc");
    }

    TEST_F(AllocationTests, TextAreasDoNotAllocateInSteadyState)
    {
        std::string notes = "A first paragraph that is long enough to wrap onto a second line.\nA second\twith a tab.";
        notes.reserve(256);
        char buffer[128] = "Line one\nLine two";
        const auto build = [&]
        {
            BeginVStack({.Spacing = 12.0f, .Padding = 20.0f});
            TextArea("Notes", &notes, {.Width = 200.0f, .Height = 60.0f, .AcceptsTab = true});
            TextArea("Buffer", buffer);
            EndVStack();
        };
        CountAllocationsOfOneFrame(build, 5);
        EXPECT_EQ(CountAllocationsOfOneFrame(build, 5), 0u);

        // Focused and typed into, then idle: the caret blinks, nothing allocates.
        GetIO().AddKeyEvent(Key::Tab, true);
        CountAllocationsOfOneFrame(build, 2);
        GetIO().AddKeyEvent(Key::Tab, false);
        GetIO().AddInputCharactersUTF8("abc");
        CountAllocationsOfOneFrame(build, 5);
        EXPECT_EQ(CountAllocationsOfOneFrame(build, 30), 0u);
        EXPECT_NE(notes.find("abc"), std::string::npos);
    }

    TEST_F(AllocationTests, ComposingDoesNotAllocateInSteadyState)
    {
        std::string notes = "Some notes";
        notes.reserve(64);
        m_Text.reserve(64);
        const auto build = [&]
        {
            BeginVStack({.Spacing = 12.0f, .Padding = 20.0f});
            TextField("Field", &m_Text, {.Width = 200.0f});
            TextArea("Notes", &notes);
            EndVStack();
        };
        const CompositionClause clauses[] = {{0, 3, true}, {3, 6, false}};
        CountAllocationsOfOneFrame(build, 5);
        for (int control = 0; control < 2; control++)
        {
            // Focused by Tab, composing: the first frames may grow buffers. Then the same pre-edit text again and
            // again, and idle frames while the caret blinks.
            GetIO().AddKeyEvent(Key::Tab, true);
            CountAllocationsOfOneFrame(build, 2);
            GetIO().AddKeyEvent(Key::Tab, false);
            GetIO().AddCompositionUpdateEvent("\xE6\xBC\xA2\xE5\xAD\x97", 6, clauses);
            CountAllocationsOfOneFrame(build, 5);
            GetIO().AddCompositionUpdateEvent("\xE6\xBC\xA2\xE5\xAD\x97", 6, clauses);
            EXPECT_EQ(CountAllocationsOfOneFrame(build, 0), 0u);
            EXPECT_EQ(CountAllocationsOfOneFrame(build, 30), 0u);
            GetIO().AddCompositionCommitEvent("x");
            CountAllocationsOfOneFrame(build, 2);
        }
        EXPECT_EQ(m_Text, "x") << "tabbing in selected everything, so the pre-edit text replaced it";
        EXPECT_EQ(notes, "Some notesx");
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
                TokenField("Tokens", &m_Tokens);
                TokenField("Tokens on one line", &m_Tokens, {.Layout = TokenFieldLayout::SingleLine});
                NumberField("Number", &m_Slider, {.Format = {.Decimals = 2, .Suffix = " px"}});
                ScrubField("Scrub", &m_Selected, {.Min = 0.0, .Max = 2.0});
                DatePicker("Date picker", &m_Date,
                           {.Elements = DatePickerElements::DateAndTime, .Format = DateFormat::US(), .Today = m_Date});
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

    TEST_F(AllocationTests, WideTablesDoNotAllocateInSteadyState)
    {
        // Forty-eight columns, scrolled sideways, while a divider is being dragged.
        static TableColumn Columns[48];
        for (TableColumn& column : Columns)
            column = {.Title = "Column", .Width = 90.0f};
        const auto build = [this]
        {
            BeginTable("wide", Columns, {.Width = 600.0f, .Height = 300.0f});
            for (int row = 0; row < 20; row++)
            {
                TableRow(row, row == m_Selected);
                for (int column = 0; column < 48; column++)
                    TableCell("Cell", {.Secondary = column % 2 == 1});
            }
            EndTable();
        };
        MoveMouse(Vec2(300.0f, 150.0f), build);
        GetIO().AddMouseWheelEvent(-20.0f, 0.0f);
        CountAllocationsOfOneFrame(build, 30);
        // The divider after the third visible column, which the table has scrolled to.
        MoveMouse(Vec2(5.0f + 90.0f * 13.0f - 960.0f, 12.0f), build);
        GetIO().AddMouseButtonEvent(MouseButton::Left, true);
        CountAllocationsOfOneFrame(build, 2);
        GetIO().AddMousePosEvent(5.0f + 90.0f * 13.0f - 900.0f, 12.0f);
        CountAllocationsOfOneFrame(build, 5);
        EXPECT_EQ(CountAllocationsOfOneFrame(build, 10), 0u);
        GetIO().AddMouseButtonEvent(MouseButton::Left, false);
        EXPECT_EQ(CountAllocationsOfOneFrame(build, 30), 0u);
        EXPECT_TRUE(m_AssertMessages.empty());
    }
#endif
} // namespace Carbon
