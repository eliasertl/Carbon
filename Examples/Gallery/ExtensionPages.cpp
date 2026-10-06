// The gallery's pages for the components of CarbonExtensions.

#include <array>
#include <format>
#include <span>
#include <string>

#include "Pages.h"

namespace Gallery
{
    using namespace Carbon;

    namespace
    {
        // Sample data for the lists, tables and charts.
        struct FileEntry
        {
            const char* Name;
            const char* Kind;
            const char* Size;
            const char* Icon;
        };

        const FileEntry Files[] = {
            {"Annual Report.pdf", "PDF document", "2.4 MB", Icons::FilePdf},
            {"Budget.xlsx", "Spreadsheet", "184 KB", Icons::FileXls},
            {"Holiday Photos", "Folder", "--", Icons::Folder},
            {"Meeting Notes.txt", "Plain text", "3 KB", Icons::FileText},
            {"Presentation.key", "Presentation", "18.1 MB", Icons::PresentationChart},
            {"Recording.m4a", "Audio", "7.9 MB", Icons::MusicNotes},
            {"Screenshots", "Folder", "--", Icons::Folder},
            {"Trailer.mov", "Movie", "241 MB", Icons::FilmStrip},
            {"Website Backup.zip", "Archive", "96.2 MB", Icons::FileZip},
        };

        const char* const Tasks[] = {"Design the icon set", "Write the layout engine",  "Shape text with HarfBuzz",
                                     "Draw squircles",      "Document every component", "Ship version 1.0"};
        const char* const Owners[] = {"Ada", "Grace", "Linus", "Ada", "Margaret", "Grace"};
        const char* const Estimates[] = {"2 d", "5 d", "3 d", "1 d", "4 d", "1 d"};

        const std::string_view Months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        const float Revenue[] = {12.0f, 14.5f, 13.2f, 17.8f, 21.0f, 19.4f, 23.5f, 26.1f, 24.0f, 28.7f, 31.2f, 34.0f};
        const float Costs[] = {9.5f, 10.2f, 11.0f, 11.4f, 12.8f, 13.1f, 13.0f, 14.6f, 15.2f, 15.0f, 16.4f, 17.1f};
        const std::string_view Quarters[] = {"Q1", "Q2", "Q3", "Q4"};
        const float Desktop[] = {42.0f, 48.0f, 45.0f, 56.0f};
        const float Mobile[] = {31.0f, 39.0f, 47.0f, 61.0f};
        const float Tablet[] = {12.0f, 11.0f, 14.0f, 13.0f};

        // A small file system for the outline and column views.
        struct FileNode
        {
            const char* Name;
            const char* Kind;
            const char* Size;
            const char* Icon;
            std::span<const FileNode> Children = {};

            bool IsFolder() const { return !Children.empty(); }
        };

        const FileNode Sources[] = {
            {"Main.cpp", "C++ source", "4 KB", Icons::FileCpp},
            {"Renderer.cpp", "C++ source", "38 KB", Icons::FileCpp},
            {"Renderer.h", "C++ header", "6 KB", Icons::FileCode},
        };
        const FileNode Assets[] = {
            {"Logo.png", "PNG image", "48 KB", Icons::FilePng},
            {"Theme.css", "Style sheet", "12 KB", Icons::FileCss},
        };
        const FileNode Project[] = {
            {"Sources", "Folder", "--", Icons::Folder, Sources},
            {"Assets", "Folder", "--", Icons::Folder, Assets},
            {"README.md", "Markdown", "2 KB", Icons::FileMd},
        };
        const FileNode Photos[] = {
            {"Beach.jpg", "JPEG image", "3.1 MB", Icons::FileImage},
            {"Mountains.jpg", "JPEG image", "4.6 MB", Icons::FileImage},
        };
        const FileNode Documents[] = {
            {"Carbon", "Folder", "--", Icons::Folder, Project},
            {"Annual Report.pdf", "PDF document", "2.4 MB", Icons::FilePdf},
            {"Budget.xlsx", "Spreadsheet", "184 KB", Icons::FileXls},
        };
        const FileNode Root[] = {
            {"Documents", "Folder", "--", Icons::Folder, Documents},
            {"Pictures", "Folder", "--", Icons::Folder, Photos},
            {"Notes.txt", "Plain text", "1 KB", Icons::FileText},
        };

        const TableColumn OutlineColumns[] = {
            {.Title = "Name"},
            {.Title = "Size", .Width = 80.0f, .Alignment = TextAlignment::Trailing},
            {.Title = "Kind", .Width = 120.0f},
        };

        void BuildOutlineNode(GalleryState& state, const FileNode& node)
        {
            const OutlineItem item =
                BeginOutlineItem(node.Name, state.OutlineSelection == &node,
                                 {.Icon = node.Icon,
                                  .HasChildren = node.IsFolder(),
                                  .IsInitiallyExpanded = std::string_view(node.Name) == "Documents"});
            OutlineCell(node.Size, {.Secondary = true});
            OutlineCell(node.Kind, {.Secondary = true});
            if (item.Picked)
                state.OutlineSelection = &node;
            if (item.Activated)
                state.OutlineAction = std::format("Opened \"{}\"", node.Name);
            if (item.IsExpanded)
            {
                for (const FileNode& child : node.Children)
                    BuildOutlineNode(state, child);
            }
            EndOutlineItem();
        }

        const std::string_view PositionNames[] = {"Top Left",    "Top Center",    "Top Right",
                                                  "Bottom Left", "Bottom Center", "Bottom Right"};

        void PostSample(GalleryState& state)
        {
            static const char* const Titles[] = {"New Message", "Export Finished", "Low Disk Space", "Upload Failed",
                                                 "Reminder"};
            static const char* const Bodies[] = {
                "Ada: Are we still on for the design review at three?",
                "Quarterly Report.pdf was saved to Documents.",
                "Only 2 GB are left on this disk. Empty the trash to free up space.",
                "Photos could not be uploaded because the connection was lost.",
                "Water the plants before you leave.",
            };
            static const NotificationStyle Styles[] = {NotificationStyle::Plain, NotificationStyle::Success,
                                                       NotificationStyle::Warning, NotificationStyle::Error,
                                                       NotificationStyle::Info};
            const int sample = state.NotificationStyle;
            NotificationOptions options;
            options.Body = Bodies[sample];
            options.Style = Styles[sample];
            if (sample == 0)
                options.Icon = Icons::ChatCircle;
            if (state.NotificationHasImage)
            {
                options.Image = WebGPUGetTextureID(state.Artwork);
                options.ImageIsRound = sample == 0;
            }
            if (state.NotificationHasActions)
            {
                static const char* const Primary[] = {"Reply", "Show", "Empty Trash", "Retry", "Done"};
                options.PrimaryAction = Primary[sample];
                options.SecondaryAction = sample == 0 ? "Mark as Read" : "";
            }
            options.Duration = state.NotificationIsSticky ? 0.0f : 6.0f;
            options.Position = static_cast<NotificationPosition>(state.NotificationPosition);
            options.Tag = static_cast<uint64_t>(sample);
            PostNotification(Titles[sample], options);
            state.NotificationsPosted++;
        }
    } // namespace

    void SelectionPage(GalleryState& state)
    {
        BeginSection("Segmented control",
                     "A set of closely related choices of which one is selected. The selection slides; the arrow "
                     "keys move it while the control has focus.");
        BeginRow("View");
        SegmentedControl("View", &state.ViewMode, {"Icons", "List", "Columns", "Gallery"});
        EndRow();
        BeginRow("Sizes");
        SegmentedControl("Small", &state.Justification, {"Left", "Center", "Right"},
                         {.ControlSize = ControlSize::Small});
        SegmentedControl("Large", &state.Justification, {"Left", "Center", "Right"},
                         {.ControlSize = ControlSize::Large});
        EndRow();
        BeginRow("Fills the row");
        SegmentedControl("Fill", &state.ViewMode, {"Icons", "List", "Columns", "Gallery"}, {.Width = Size::Fill()});
        EndRow();
        BeginRow("Disabled");
        SegmentedControl("Disabled", &state.ViewMode, {"Icons", "List", "Columns", "Gallery"}, {.Disabled = true});
        EndRow();
        EndSection();

        BeginSection("Radio buttons",
                     "Two to five mutually exclusive choices. The group is one stop for Tab; the arrow keys move "
                     "the selection.");
        // A grid with top-aligned rows keeps each label on the line of the group's first button.
        BeginGrid({.HorizontalSpacing = 12.0f, .VerticalSpacing = 14.0f, .VerticalAlignment = VerticalAlignment::Top});
        BeginGridRow();
        Text("Appearance", {.Secondary = true, .Width = LabelColumn});
        RadioGroup("Appearance", &state.Appearance, {"Light", "Dark", "Automatic"});
        EndGridRow();
        BeginGridRow();
        Text("Icon size", {.Secondary = true, .Width = LabelColumn});
        RadioGroup("Icon size", &state.IconSize, {"Small", "Medium", "Large"}, {.Orientation = Axis::Horizontal});
        EndGridRow();
        BeginGridRow();
        Text("Nothing selected yet", {.Secondary = true, .Width = LabelColumn});
        RadioGroup("Start with", &state.StartWith, {"A new window", "The last session"});
        EndGridRow();
        BeginGridRow();
        Text("Disabled", {.Secondary = true, .Width = LabelColumn});
        RadioGroup("Disabled##radio", &state.IconSize, {"Small", "Medium", "Large"},
                   {.Orientation = Axis::Horizontal, .Disabled = true});
        EndGridRow();
        EndGrid();
        EndSection();

        BeginSection("Pop-up and pull-down buttons",
                     "A pop-up button picks one value from a list and shows it. A pull-down button offers "
                     "commands and keeps its title.");
        BeginRow("Sort by");
        PopUpButton("Sort", &state.SortOrder, {"Name", "Date Modified", "Size", "Kind"});
        PopUpButton("Quality", &state.Quality, {"Draft", "Normal", "Best"}, {.ControlSize = ControlSize::Small});
        EndRow();
        BeginRow("Commands");
        if (BeginPullDownButton("Add", {.Icon = Icons::Plus}))
        {
            if (MenuItem("New Folder", {.Icon = Icons::FolderPlus, .Shortcut = "Ctrl+Shift+N"}))
                state.LastAction = "New Folder";
            if (MenuItem("New Document", {.Icon = Icons::FilePlus, .Shortcut = "Ctrl+N"}))
                state.LastAction = "New Document";
            MenuSeparator();
            if (MenuItem("Import...", {.Icon = Icons::DownloadSimple}))
                state.LastAction = "Import";
            EndPullDownButton();
        }
        Text(std::format("Last command: {}", state.LastAction), {.Secondary = true});
        EndRow();
        EndSection();

        BeginSection("Combo box",
                     "A text field with a list of choices: type any value, or pick one. While you type, the list "
                     "shows the matching choices; the arrow keys move through it and Enter picks.");
        static const std::string_view Fonts[] = {"Courier",  "Georgia",  "Helvetica", "Menlo",        "Public Sans",
                                                 "Palatino", "Rockwell", "Times",     "Trebuchet MS", "Verdana"};
        static const std::string_view Cities[] = {"Amsterdam", "Berlin", "Copenhagen", "Lisbon", "London",
                                                  "Madrid",    "Paris",  "Prague",     "Rome",   "Vienna"};
        BeginRow("Font:");
        ComboBox("Font", &state.FontName, Fonts, {.Width = 200.0f});
        Text(std::format("\"{}\"", state.FontName), {.Secondary = true});
        EndRow();
        BeginRow("City:");
        ComboBox("City", &state.City, Cities, {.Placeholder = "Choose or type a city", .Width = 200.0f});
        EndRow();
        BeginRow("Disabled");
        ComboBox("Disabled", &state.FontName, Fonts, {.Width = 200.0f, .Disabled = true});
        EndRow();
        EndSection();

        BeginSection("Token field",
                     "Text that turns into tokens at a comma or Return, as for the recipients of a mail. Click a "
                     "token to select it, or press Backspace at the start of the text; Backspace again deletes it. "
                     "Right-click a token for its menu.");
        BeginRow("To:");
        TokenField("To", &state.Recipients, {.Placeholder = "Add recipients", .Width = 340.0f});
        int token = 0;
        if (BeginTokenFieldMenu("To", &token))
        {
            if (MenuItem("Copy Name", {.Icon = Icons::Copy}))
                state.TokenAction = std::format("Copied \"{}\"", state.Recipients[static_cast<size_t>(token)]);
            MenuSeparator();
            if (MenuItem("Remove", {.Icon = Icons::Trash, .IsDestructive = true}))
                state.Recipients.erase(state.Recipients.begin() + token);
            EndTokenFieldMenu();
        }
        EndRow();
        BeginRow("Tags, one line");
        TokenField("Tags", &state.Tags,
                   {.Placeholder = "Add tags", .Layout = TokenFieldLayout::SingleLine, .Width = 340.0f});
        EndRow();
        BeginRow("Disabled");
        TokenField("Disabled tags", &state.Tags, {.Width = 340.0f, .Disabled = true});
        EndRow();
        Text(std::format("Last action: {}", state.TokenAction), {.Secondary = true});
        EndSection();

        BeginSection("Stepper", "Small steps on a value that is shown next to it. Hold a button to repeat.");
        BeginRow("Copies");
        Text(std::format("{}", state.Copies), {.Width = 24.0f, .Alignment = TextAlignment::Trailing});
        Stepper("Copies", &state.Copies, {.Min = 1.0, .Max = 99.0});
        EndRow();
        BeginRow("Font size");
        Text(std::format("{:.1f} pt", state.FontSize), {.Width = 52.0f, .Alignment = TextAlignment::Trailing});
        Stepper("Font size", &state.FontSize, {.Min = 8.0, .Max = 72.0, .Step = 0.5});
        Text("Sample", {.Color = GetStyleColor(StyleColor::SecondaryLabel)});
        EndRow();
        EndSection();

        BeginSection("Color well", "Shows a color and opens a popover to change it.");
        BeginRow("Tint");
        ColorWell("Tint", &state.Tint);
        Button("Tinted", {.Role = ButtonRole::Prominent, .Tint = state.Tint});
        EndRow();
        BeginRow("With opacity");
        ColorWell("Shadow", &state.Shadow, {.ShowsOpacity = true});
        Text(std::format("{:.0f} % opaque", state.Shadow.A * 100.0f), {.Secondary = true});
        EndRow();
        EndSection();

        BeginSection("Search field", "A text field for search terms. Escape clears it.");
        BeginRow("Search");
        SearchField("Search", &state.Query, {.Width = 240.0f});
        if (!state.Query.empty())
            Text(std::format("Searching for \"{}\"", state.Query), {.Secondary = true});
        EndRow();
        EndSection();
    }

    void DatesPage(GalleryState& state)
    {
        BeginSection("Date picker",
                     "The textual date picker. Click an element or move between them with the left and right arrow "
                     "keys, then type or step it with the up and down arrow keys or the stepper. Clicking the date "
                     "opens a calendar.");
        BeginGrid({.HorizontalSpacing = 12.0f, .VerticalSpacing = 12.0f});
        BeginGridRow();
        Text("ISO date", {.Secondary = true, .Width = LabelColumn});
        if (TakeShow(state, "datepicker"))
        {
            // The popover stays open while the field has the focus.
            OpenOverlay(HashID("##popover", GetID("ISO date")));
            SetFocus(GetID("ISO date"));
        }
        DatePicker("ISO date", &state.Appointment, {.Today = state.Today});
        EndGridRow();
        BeginGridRow();
        Text("German, with time", {.Secondary = true, .Width = LabelColumn});
        DatePicker("German date", &state.Appointment,
                   {.Elements = DatePickerElements::DateAndTime, .Format = DateFormat::German(), .Today = state.Today});
        EndGridRow();
        BeginGridRow();
        Text("US, with time", {.Secondary = true, .Width = LabelColumn});
        DatePicker("US date", &state.Appointment,
                   {.Elements = DatePickerElements::DateAndTime,
                    .Format = DateFormat::US(),
                    .FirstWeekday = Weekday::Sunday,
                    .Today = state.Today});
        EndGridRow();
        BeginGridRow();
        Text("Time, quarter hours", {.Secondary = true, .Width = LabelColumn});
        DatePicker("Alarm", &state.Alarm,
                   {.Elements = DatePickerElements::Time, .MinuteInterval = 15, .Today = state.Today});
        EndGridRow();
        BeginGridRow();
        Text("Within October", {.Secondary = true, .Width = LabelColumn});
        DatePicker("Deadline", &state.Deadline,
                   {.Elements = DatePickerElements::DateAndTime,
                    .MinDate = DateTime{.Year = 2026, .Month = 10, .Day = 1},
                    .MaxDate = DateTime{.Year = 2026, .Month = 10, .Day = 31, .Hour = 23, .Minute = 59},
                    .Today = state.Today});
        EndGridRow();
        EndGrid();
        EndSection();

        BeginSection("Calendar",
                     "The graphical date picker. Click a day, or use the arrow keys while it has focus; Page Up and "
                     "Page Down change the month, with Shift the year. Today is marked in the accent color.");
        BeginHStack({.Spacing = 40.0f, .Alignment = VerticalAlignment::Top});
        BeginVStack({.Spacing = 8.0f});
        Text("Weeks start on Monday", {.Style = TextStyle::Subheadline, .Secondary = true});
        DatePickerCalendar("Calendar", &state.CalendarDate, {.Today = state.Today});
        EndVStack();
        BeginVStack({.Spacing = 8.0f});
        Text("Weeks start on Sunday", {.Style = TextStyle::Subheadline, .Secondary = true});
        DatePickerCalendar("Sunday calendar", &state.SundayDate,
                           {.FirstWeekday = Weekday::Sunday, .Today = state.Today});
        EndVStack();
        BeginVStack({.Spacing = 8.0f});
        Text("October 3 to 24 only", {.Style = TextStyle::Subheadline, .Secondary = true});
        DatePickerCalendar("Limited calendar", &state.LimitedDate,
                           {.MinDate = DateTime{.Year = 2026, .Month = 10, .Day = 3},
                            .MaxDate = DateTime{.Year = 2026, .Month = 10, .Day = 24, .Hour = 23, .Minute = 59},
                            .Today = state.Today});
        EndVStack();
        EndHStack();
        const DateTime& date = state.CalendarDate;
        Text(std::format("Selected: {}, {} {}, {}", GetWeekdayName(GetWeekday(date)), GetMonthName(date.Month),
                         date.Day, date.Year),
             {.Secondary = true});
        EndSection();
    }

    void MenusPage(GalleryState& state)
    {
        BeginSection("Menu bar",
                     "The menus of a window, in a row at its top. Click a title, then move along the bar: the menus "
                     "follow the pointer. Alt or F10 opens the first menu; the arrow keys move between menus.");
        BeginVStack({.Spacing = 0.0f,
                     .Width = Size::Fill(),
                     .Background = GetStyleColor(StyleColor::Background),
                     .CornerRadius = 8.0f,
                     .ID = "window"});
        BeginMenuBar({.Background = Color::Transparent()});
        if (BeginMenuBarMenu("File"))
        {
            if (MenuItem("New", {.Icon = Icons::FilePlus, .Shortcut = "Ctrl+N"}))
                state.MenuBarAction = "New";
            if (MenuItem("Open...", {.Icon = Icons::FolderOpen, .Shortcut = "Ctrl+O"}))
                state.MenuBarAction = "Open";
            if (BeginSubmenu("Open Recent"))
            {
                if (MenuItem("Quarterly Report.pdf"))
                    state.MenuBarAction = "Open Quarterly Report.pdf";
                if (MenuItem("Budget.xlsx"))
                    state.MenuBarAction = "Open Budget.xlsx";
                EndSubmenu();
            }
            MenuSeparator();
            if (MenuItem("Save", {.Shortcut = "Ctrl+S"}))
                state.MenuBarAction = "Save";
            if (MenuItem("Export As...", {.Icon = Icons::Export}))
                state.MenuBarAction = "Export As";
            MenuSeparator();
            if (MenuItem("Close Window", {.Shortcut = "Ctrl+W"}))
                state.MenuBarAction = "Close Window";
            EndMenuBarMenu();
        }
        if (BeginMenuBarMenu("Edit"))
        {
            if (MenuItem("Undo Typing", {.Icon = Icons::ArrowCounterClockwise, .Shortcut = "Ctrl+Z"}))
                state.MenuBarAction = "Undo Typing";
            MenuItem("Redo", {.Icon = Icons::ArrowClockwise, .Shortcut = "Ctrl+Y", .Disabled = true});
            MenuSeparator();
            if (MenuItem("Cut", {.Icon = Icons::Scissors, .Shortcut = "Ctrl+X"}))
                state.MenuBarAction = "Cut";
            if (MenuItem("Copy", {.Icon = Icons::Copy, .Shortcut = "Ctrl+C"}))
                state.MenuBarAction = "Copy";
            if (MenuItem("Paste", {.Icon = Icons::ClipboardText, .Shortcut = "Ctrl+V"}))
                state.MenuBarAction = "Paste";
            MenuSeparator();
            if (MenuItem("Select All", {.Shortcut = "Ctrl+A"}))
                state.MenuBarAction = "Select All";
            EndMenuBarMenu();
        }
        if (BeginMenuBarMenu("View"))
        {
            // Show/Hide items name what they will do, as the HIG asks.
            if (MenuItem(state.ShowsToolbar ? "Hide Toolbar" : "Show Toolbar"))
                state.ShowsToolbar = !state.ShowsToolbar;
            if (MenuItem(state.ShowsStatusBar ? "Hide Status Bar" : "Show Status Bar"))
                state.ShowsStatusBar = !state.ShowsStatusBar;
            MenuSeparator();
            if (MenuItem("Enter Full Screen", {.Shortcut = "F11"}))
                state.MenuBarAction = "Enter Full Screen";
            EndMenuBarMenu();
        }
        if (BeginMenuBarMenu("Window"))
        {
            if (MenuItem("Minimize", {.Shortcut = "Ctrl+M"}))
                state.MenuBarAction = "Minimize";
            if (MenuItem("Zoom"))
                state.MenuBarAction = "Zoom";
            EndMenuBarMenu();
        }
        if (BeginMenuBarMenu("Help"))
        {
            if (MenuItem("Gallery Help", {.Icon = Icons::Question}))
                state.MenuBarAction = "Gallery Help";
            EndMenuBarMenu();
        }
        EndMenuBar();
        BeginVStack({.Spacing = 6.0f, .Padding = 16.0f, .Width = Size::Fill()});
        if (state.ShowsToolbar)
            Text("Toolbar", {.Style = TextStyle::Subheadline, .Secondary = true});
        Text(std::format("Last command: {}", state.MenuBarAction));
        if (state.ShowsStatusBar)
            Text("Status bar", {.Style = TextStyle::Subheadline, .Secondary = true});
        EndVStack();
        EndVStack();
        EndSection();

        BeginSection("Menu",
                     "Commands on demand. Arrow keys move the highlight, Enter chooses, the right arrow opens a "
                     "submenu, Escape closes.");
        BeginRow("Menu");
        if (Button("Edit") || TakeShow(state, "menu"))
            OpenMenu("edit");
        if (BeginMenu("edit"))
        {
            if (MenuItem("Undo", {.Icon = Icons::ArrowCounterClockwise, .Shortcut = "Ctrl+Z"}))
                state.LastAction = "Undo";
            MenuItem("Redo", {.Icon = Icons::ArrowClockwise, .Shortcut = "Ctrl+Y", .Disabled = true});
            MenuSeparator();
            if (MenuItem("Cut", {.Icon = Icons::Scissors, .Shortcut = "Ctrl+X"}))
                state.LastAction = "Cut";
            if (MenuItem("Copy", {.Icon = Icons::Copy, .Shortcut = "Ctrl+C"}))
                state.LastAction = "Copy";
            if (MenuItem("Paste", {.Icon = Icons::ClipboardText, .Shortcut = "Ctrl+V"}))
                state.LastAction = "Paste";
            MenuSeparator();
            if (BeginSubmenu("Share", {.Icon = Icons::Export}))
            {
                if (MenuItem("Mail", {.Icon = Icons::Envelope}))
                    state.LastAction = "Share by Mail";
                if (MenuItem("Messages", {.Icon = Icons::ChatCircle}))
                    state.LastAction = "Share by Messages";
                if (BeginSubmenu("More"))
                {
                    if (MenuItem("Copy Link"))
                        state.LastAction = "Copy Link";
                    if (MenuItem("Add to Reading List"))
                        state.LastAction = "Add to Reading List";
                    EndSubmenu();
                }
                EndSubmenu();
            }
            MenuSeparator();
            MenuHeader("View");
            if (MenuItem("Show Ruler", {.IsChecked = state.ShowRuler}))
                state.ShowRuler = !state.ShowRuler;
            if (MenuItem("Show Grid", {.IsChecked = state.ShowGrid}))
                state.ShowGrid = !state.ShowGrid;
            MenuSeparator();
            if (MenuItem("Delete", {.Icon = Icons::Trash, .IsDestructive = true}))
                state.LastAction = "Delete";
            EndMenu();
        }
        Text(std::format("Last command: {}", state.LastAction), {.Secondary = true});
        EndRow();
        EndSection();

        BeginSection("Context menu", "A menu that opens at the pointer on a right click.");
        BeginHStack({.Padding = EdgeInsets(16.0f, 22.0f),
                     .Justify = Alignment::Center,
                     .Width = Size::Fill(),
                     .Background = GetStyleColor(StyleColor::ControlFill),
                     .CornerRadius = 8.0f,
                     .ID = "context area"});
        Text("Right-click anywhere in this area", {.Secondary = true});
        EndHStack();
        // The context menu belongs to the item submitted last. Make that the whole area, not just its text.
        const Rect area = GetLastItemRect();
        Interaction areaInteraction;
        areaInteraction.Hovered = IsRectHovered(area);
        SetLastItem(GetID("context area"), area, areaInteraction);
        if (BeginContextMenu("context"))
        {
            if (MenuItem("Open"))
                state.LastAction = "Open";
            if (MenuItem("Get Info", {.Shortcut = "Ctrl+I"}))
                state.LastAction = "Get Info";
            MenuSeparator();
            if (MenuItem("Rename"))
                state.LastAction = "Rename";
            if (MenuItem("Move to Trash", {.IsDestructive = true}))
                state.LastAction = "Move to Trash";
            EndContextMenu();
        }
        EndSection();

        BeginSection("Popover",
                     "A transient view attached to the control it belongs to. It closes with a click outside "
                     "or with Escape, and flips to the other side when there is no room.");
        BeginRow("Below");
        if (Button("Sound", {.Icon = Icons::SpeakerHigh}) || TakeShow(state, "popover"))
            OpenPopover("sound");
        if (BeginPopover("sound", {.Width = 240.0f}))
        {
            Text("Sound", {.Style = TextStyle::Headline});
            BeginHStack({.Spacing = 8.0f, .Width = Size::Fill()});
            Icon(Icons::SpeakerLow, {.Color = GetStyleColor(StyleColor::SecondaryLabel)});
            Slider("Volume", &state.PopoverVolume, 0.0f, 1.0f, {.Width = Size::Fill()});
            Icon(Icons::SpeakerHigh, {.Color = GetStyleColor(StyleColor::SecondaryLabel)});
            EndHStack();
            Separator();
            Toggle("Play feedback sounds", &state.Notifications, {.Width = Size::Fill()});
            EndPopover();
        }
        EndRow();
        BeginRow("Other sides");
        if (Button("Trailing"))
            OpenPopover("trailing");
        if (BeginPopover("trailing", {.Placement = OverlayPlacement::Trailing}))
        {
            Text("Popovers point at their anchor.");
            EndPopover();
        }
        if (Button("Above"))
            OpenPopover("above");
        if (BeginPopover("above", {.Placement = OverlayPlacement::Above}))
        {
            Text("This one prefers to sit above.");
            EndPopover();
        }
        EndRow();
        EndSection();
    }

    namespace
    {
        // The same toolbar in every display mode: a toggle, navigation, a control, commands and a search field.
        void DemoToolbar(GalleryState& state, ToolbarDisplayMode mode, Size width)
        {
            BeginToolbar("Toolbar", {.Width = width,
                                     .Background = GetStyleColor(StyleColor::Background),
                                     .HasSeparator = false,
                                     .DisplayMode = mode});
            if (ToolbarItem("Sidebar", {.Icon = Icons::SidebarSimple, .IsSelected = state.ToolbarShowsSidebar}))
                state.ToolbarShowsSidebar = !state.ToolbarShowsSidebar;
            ToolbarSpace();
            if (ToolbarItem("Back", {.Icon = Icons::CaretLeft}))
                state.ToolbarAction = "Back";
            ToolbarItem("Forward", {.Icon = Icons::CaretRight, .Disabled = true});
            ToolbarSeparator();
            if (BeginToolbarControl("View", {.Icon = Icons::SquaresFour}))
            {
                SegmentedControl("View", &state.ToolbarView, {"Icons", "List", "Columns"});
                EndToolbarControl();
            }
            ToolbarFlexibleSpace();
            if (ToolbarItem("New Folder", {.Icon = Icons::FolderPlus}))
                state.ToolbarAction = "New Folder";
            if (ToolbarItem("Share", {.Icon = Icons::Export}))
                state.ToolbarAction = "Share";
            if (ToolbarItem("Delete", {.Icon = Icons::Trash}))
                state.ToolbarAction = "Delete";
            if (BeginToolbarControl("Sort", {.Icon = Icons::ArrowsDownUp}))
            {
                if (BeginPullDownButton("Sort", {.Icon = Icons::ArrowsDownUp}))
                {
                    if (MenuItem("Name"))
                        state.ToolbarAction = "Sort by name";
                    if (MenuItem("Date Modified"))
                        state.ToolbarAction = "Sort by date";
                    EndPullDownButton();
                }
                EndToolbarControl();
            }
            ToolbarSpace();
            if (BeginToolbarControl("Search", {.Icon = Icons::MagnifyingGlass}))
            {
                SearchField("Search", &state.ToolbarQuery, {.Width = 150.0f});
                EndToolbarControl();
            }
            EndToolbar();
        }
    } // namespace

    void ToolbarsPage(GalleryState& state)
    {
        BeginSection("Display modes",
                     "Frequently used commands and controls. Items have no bezel: a highlight appears under the "
                     "pointer. The sidebar item toggles; Forward is disabled. Every item is a stop for Tab.");
        static const ToolbarDisplayMode Modes[] = {ToolbarDisplayMode::IconAndLabel, ToolbarDisplayMode::IconOnly,
                                                   ToolbarDisplayMode::LabelOnly};
        static const std::string_view Names[] = {"Icon and label", "Icon only (labels become tooltips)", "Label only"};
        for (int i = 0; i < 3; i++)
        {
            PushID(i);
            Text(Names[i], {.Style = TextStyle::Subheadline, .Secondary = true});
            DemoToolbar(state, Modes[i], Size::Fill());
            PopID();
        }
        Text(std::format("Last action: {}", state.ToolbarAction), {.Secondary = true});
        EndSection();

        BeginSection("Overflow",
                     "When the toolbar is too narrow for its items, those at its trailing end move into a menu "
                     "behind the chevron. A control chosen there opens in a popover.");
        PushID("narrow");
        if (TakeShow(state, "toolbaroverflow"))
            OpenOverlay(HashID("##overflowmenu", GetID("Toolbar")));
        DemoToolbar(state, ToolbarDisplayMode::IconAndLabel, 380.0f);
        PopID();
        EndSection();
    }

    void DialogsPage(GalleryState& state)
    {
        BeginSection("Alert",
                     "Critical information that needs an answer. Enter chooses the default button, Escape "
                     "cancels. A destructive action is never the default.");
        BeginRow("Alerts");
        if (Button("Delete File...", {.Role = ButtonRole::Destructive}) || TakeShow(state, "alert"))
            OpenAlert("delete");
        switch (Alert("delete", "Delete \"Budget.xlsx\"?",
                      {.Message = "The file is deleted immediately. You cannot undo this action.",
                       .Icon = Icons::Warning,
                       .PrimaryLabel = "Delete",
                       .SecondaryLabel = "Cancel",
                       .IsDestructive = true}))
        {
            case AlertResult::Primary:
                state.AlertAnswer = "Delete";
                break;
            case AlertResult::Secondary:
                state.AlertAnswer = "Cancel";
                break;
            case AlertResult::None:
                break;
        }

        if (Button("Show Message"))
            OpenAlert("message");
        if (Alert("message", "The update is ready",
                  {.Message = "Carbon will restart to finish installing it.", .Icon = Icons::Info}) !=
            AlertResult::None)
            state.AlertAnswer = "OK";
        Text(std::format("Last answer: {}", state.AlertAnswer), {.Secondary = true});
        EndRow();
        EndSection();

        BeginSection("Sheet",
                     "A modal view for a task of its own. The interface beneath is dimmed and does not react "
                     "until the sheet is closed.");
        BeginRow("Sheet");
        if (Button("Export...") || TakeShow(state, "sheet"))
            OpenSheet("export");
        if (BeginSheet("export", {.Width = 400.0f}))
        {
            Text("Export", {.Style = TextStyle::Title2, .Emphasized = true});
            Text("Choose a name and a format for the exported file.", {.Secondary = true});
            Spacer({.Length = 4.0f});

            BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
            Text("Name", {.Secondary = true, .Width = 70.0f});
            TextField("Name", &state.ExportName, {.Width = Size::Fill()});
            EndHStack();
            BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
            Text("Format", {.Secondary = true, .Width = 70.0f});
            PopUpButton("Format", &state.ExportFormat, {"PNG image", "JPEG image", "PDF document"});
            EndHStack();
            BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
            Text("", {.Width = 70.0f});
            Toggle("Transparent background", &state.ExportTransparent, {.Kind = ToggleKind::Checkbox});
            EndHStack();

            Spacer({.Length = 8.0f});
            BeginHStack({.Spacing = 8.0f, .Width = Size::Fill()});
            Spacer();
            if (Button("Cancel"))
                CloseCurrentSheet();
            if (Button("Export", {.Role = ButtonRole::Prominent, .IsDefault = true}))
            {
                state.AlertAnswer = std::format("Exported \"{}\"", state.ExportName);
                CloseCurrentSheet();
            }
            EndHStack();
            EndSheet();
        }
        EndRow();
        EndSection();
    }

    void NotificationsPage(GalleryState& state)
    {
        BeginSection("Notifications",
                     "Banners that tell about something that happened and go away by themselves. The time stops "
                     "while the pointer rests on one; a close button appears in its corner.");
        BeginRow("Position");
        PopUpButton("Position", &state.NotificationPosition, PositionNames);
        EndRow();
        BeginRow("Example");
        SegmentedControl("Example", &state.NotificationStyle, {"Message", "Success", "Warning", "Error", "Info"});
        EndRow();
        BeginRow("Options");
        Toggle("Image", &state.NotificationHasImage, {.Kind = ToggleKind::Checkbox});
        Toggle("Actions", &state.NotificationHasActions, {.Kind = ToggleKind::Checkbox});
        Toggle("Stays until closed", &state.NotificationIsSticky, {.Kind = ToggleKind::Checkbox});
        EndRow();
        BeginRow("");
        if (Button("Post Notification", {.Role = ButtonRole::Prominent}) || TakeShow(state, "notification"))
            PostSample(state);
        if (Button("Dismiss All", {.Disabled = GetNotificationCount() == 0}))
            DismissAllNotifications();
        EndRow();
        BeginRow("Last event");
        Text(state.NotificationEvent, {.Secondary = true});
        EndRow();
        EndSection();
    }

    void ShowGalleryNotifications(GalleryState& state)
    {
        static const char* const Kinds[] = {"clicked", "primary action", "secondary action", "closed", "expired"};
        // Kept in a variable rather than looped over directly: GCC 13 warns about a dangling reference otherwise.
        const std::span<const NotificationEvent> events = ShowNotifications();
        for (const NotificationEvent& event : events)
        {
            state.NotificationEvent =
                std::format("Notification {} {}", event.Tag + 1, Kinds[static_cast<int>(event.Kind)]);
        }
    }

    void ProgressPage(GalleryState& state)
    {
        BeginSection("Progress bar", "For a task whose length is known, or indeterminate while it is not.");
        BeginRow("Progress");
        ProgressIndicator(state.Progress, {.Width = Size::Fill()});
        Text(std::format("{:.0f} %", state.Progress * 100.0f),
             {.Secondary = true, .Width = 40.0f, .Alignment = TextAlignment::Trailing});
        EndRow();
        BeginRow("Set the value");
        Slider("Progress", &state.Progress, 0.0f, 1.0f, {.Width = Size::Fill()});
        EndRow();
        BeginRow("Indeterminate");
        ProgressIndicator(0.0f, {.IsIndeterminate = true, .Width = Size::Fill()});
        EndRow();
        BeginRow("Sizes and tint");
        BeginVStack({.Spacing = 10.0f, .Width = Size::Fill()});
        ProgressIndicator(state.Progress, {.Width = Size::Fill(), .ControlSize = ControlSize::Small});
        ProgressIndicator(state.Progress, {.Width = Size::Fill(), .Tint = GetStyleColor(StyleColor::Green)});
        ProgressIndicator(
            state.Progress,
            {.Width = Size::Fill(), .ControlSize = ControlSize::Large, .Tint = GetStyleColor(StyleColor::Orange)});
        EndVStack();
        EndRow();
        EndSection();

        BeginSection("Spinner", "For short waits where a bar would be too much.");
        BeginRow("Indeterminate");
        ProgressIndicator(0.0f,
                          {.Kind = ProgressKind::Spinner, .IsIndeterminate = true, .ControlSize = ControlSize::Small});
        ProgressIndicator(0.0f, {.Kind = ProgressKind::Spinner, .IsIndeterminate = true});
        ProgressIndicator(0.0f,
                          {.Kind = ProgressKind::Spinner, .IsIndeterminate = true, .ControlSize = ControlSize::Large});
        Text("Loading...", {.Secondary = true});
        EndRow();
        BeginRow("Determinate");
        ProgressIndicator(state.Progress, {.Kind = ProgressKind::Spinner});
        ProgressIndicator(state.Progress, {.Kind = ProgressKind::Spinner, .ControlSize = ControlSize::Large});
        ProgressIndicator(state.Progress, {.Kind = ProgressKind::Spinner,
                                           .ControlSize = ControlSize::Large,
                                           .Tint = GetStyleColor(StyleColor::Green)});
        EndRow();
        EndSection();
    }

    void ListsPage(GalleryState& state)
    {
        BeginSection("List",
                     "Rows of which the application marks any as selected. Click a row, or give the list focus "
                     "and use the arrow keys.");
        BeginList("files", {.Height = 196.0f});
        for (int i = 0; i < static_cast<int>(std::size(Files)); i++)
        {
            PushID(i);
            if (ListItem(Files[i].Name, i == state.SelectedFile, {.Icon = Files[i].Icon, .Detail = Files[i].Size}))
                state.SelectedFile = i;
            PopID();
        }
        EndList();
        Text(std::format("Selected: {}", Files[state.SelectedFile].Name), {.Secondary = true});
        EndSection();

        BeginSection("Table", "Rows in columns. Cells are text, or anything else.");
        const TableColumn columns[] = {{.Title = "Done", .Width = 52.0f},
                                       {.Title = "Task"},
                                       {.Title = "Owner", .Width = 110.0f},
                                       {.Title = "Estimate", .Width = 80.0f, .Alignment = TextAlignment::Trailing}};
        BeginTable("tasks", columns, {.Height = 190.0f});
        for (int i = 0; i < static_cast<int>(std::size(Tasks)); i++)
        {
            if (TableRow(i, i == state.SelectedTask))
                state.SelectedTask = i;
            BeginTableCell();
            Toggle("##done", &state.TaskDone[i], {.Kind = ToggleKind::Checkbox});
            EndTableCell();
            TableCell(Tasks[i]);
            TableCell(Owners[i], {.Icon = Icons::User});
            TableCell(Estimates[i], {.Secondary = true});
        }
        EndTable();
        EndSection();
    }

    void HierarchiesPage(GalleryState& state)
    {
        BeginSection("Outline view",
                     "Hierarchical data in rows: click a disclosure triangle, or use the left and right arrow keys, "
                     "to expand and collapse. Alt-click expands everything inside. Double-click or Enter opens.");
        BeginOutlineView("files", {.Height = 250.0f, .Columns = OutlineColumns, .ShowsAlternatingRows = true});
        for (const FileNode& node : Root)
            BuildOutlineNode(state, node);
        EndOutlineView();
        Text(std::format("Last action: {}", state.OutlineAction), {.Secondary = true});
        EndSection();

        BeginSection("Column view",
                     "Each level of a hierarchy in its own column, as in a file browser. The arrow keys move within "
                     "and between columns; drag the lines between columns to resize them.");
        BeginColumnView("browser", {.Height = 220.0f, .ColumnWidth = 180.0f});
        std::span<const FileNode> level = Root;
        const FileNode* selected = nullptr;
        for (size_t depth = 0; !level.empty(); depth++)
        {
            BeginColumnViewColumn();
            std::span<const FileNode> next;
            for (int i = 0; i < static_cast<int>(level.size()); i++)
            {
                const FileNode& node = level[static_cast<size_t>(i)];
                const bool isSelected = depth < state.ColumnPath.size() && state.ColumnPath[depth] == i;
                if (ColumnViewItem(node.Name, isSelected, {.Icon = node.Icon, .HasChildren = node.IsFolder()}))
                {
                    // Picking an item makes it the end of the path.
                    state.ColumnPath.resize(depth);
                    state.ColumnPath.push_back(i);
                }
                if (isSelected)
                {
                    selected = &node;
                    next = node.Children;
                }
            }
            EndColumnViewColumn();
            level = next;
        }
        if (selected != nullptr && !selected->IsFolder())
        {
            BeginColumnViewPreview();
            Spacer({.Length = 12.0f});
            Icon(selected->Icon, {.Size = 56.0f, .Color = GetStyleColor(StyleColor::Accent)});
            Text(selected->Name, {.Style = TextStyle::Headline});
            Text(std::format("{} - {}", selected->Kind, selected->Size), {.Secondary = true});
            EndColumnViewPreview();
        }
        EndColumnView();
        EndSection();

        BeginSection("Path control",
                     "The path to the item selected in the column view above. Click a component to go back to it. "
                     "When the path does not fit, the names between the first and the last component make room; "
                     "point at one to see it.");
        // The path is the column view's: the disk, then one component per selected item.
        PathControlItem path[16];
        size_t length = 0;
        path[length++] = {.Label = "Macintosh HD", .Icon = Icons::HardDrives};
        std::span<const FileNode> nodes = Root;
        for (const int index : state.ColumnPath)
        {
            if (index < 0 || static_cast<size_t>(index) >= nodes.size() || length == std::size(path))
                break;
            const FileNode& node = nodes[static_cast<size_t>(index)];
            path[length++] = {.Label = node.Name, .Icon = node.Icon};
            nodes = node.Children;
        }
        const std::span<const PathControlItem> components(path, length);
        const auto goTo = [&](int component)
        {
            if (component >= 0)
                state.ColumnPath.resize(static_cast<size_t>(component));
        };
        BeginRow("Standard");
        goTo(PathControl("Path", components, {.Width = Size::Fill()}));
        EndRow();
        BeginRow("Narrow");
        goTo(PathControl("Narrow path", components, {.Width = 240.0f}));
        EndRow();
        BeginRow("Pop-up");
        if (TakeShow(state, "pathmenu"))
            OpenOverlay(HashID("##menu", GetID("Location")));
        goTo(PathControl("Location", components, {.Style = PathControlStyle::PopUp}));
        EndRow();
        EndSection();
    }

    void NavigationPage(GalleryState& state)
    {
        BeginSection("Tab view", "Several panes in the same place, switched with a segmented control.");
        BeginTabView("settings", &state.Tab, {"General", "Appearance", "Advanced"});
        if (state.Tab == 0)
        {
            Toggle("Open windows from the last session", &state.AutoSave, {.Width = Size::Fill()});
            Separator();
            Toggle("Show hidden files", &state.ShowHidden, {.Width = Size::Fill()});
        }
        else if (state.Tab == 1)
        {
            BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
            Text("Accent color", {.Width = 120.0f});
            ColorWell("Accent", &state.Tint);
            EndHStack();
            BeginHStack({.Spacing = 12.0f, .Width = Size::Fill()});
            Text("Text size", {.Width = 120.0f});
            Slider("Text size", &state.Brightness, 0.0f, 1.0f, {.Width = Size::Fill()});
            EndHStack();
        }
        else
        {
            Text("Nothing here needs changing.", {.Secondary = true});
            Button("Reset All Settings", {.Role = ButtonRole::Destructive});
        }
        EndTabView();
        EndSection();

        BeginSection("Split view",
                     "Panes with a divider that can be dragged, or moved with the arrow keys while it has "
                     "focus. Split views nest.");
        BeginVStack({.Width = Size::Fill(),
                     .Height = 240.0f,
                     .Background = GetStyleColor(StyleColor::Background),
                     .CornerRadius = 8.0f});
        BeginSplitView("mail", {.InitialSize = 170.0f, .MinSize = 110.0f});
        {
            static const char* const Mailboxes[] = {"Inbox", "Drafts", "Sent", "Archive"};
            static const char* const MailboxIcons[] = {Icons::Tray, Icons::PencilSimple, Icons::PaperPlaneTilt,
                                                       Icons::Archive};
            BeginList("mailboxes", {.HasBorder = false});
            for (int i = 0; i < 4; i++)
            {
                if (ListItem(Mailboxes[i], i == state.Mailbox, {.Icon = MailboxIcons[i]}))
                    state.Mailbox = i;
            }
            EndList();
        }
        SplitViewDivider();
        {
            BeginSplitView("message",
                           {.Axis = Axis::Vertical, .InitialSize = 90.0f, .MinSize = 50.0f, .MinSecondSize = 60.0f});
            BeginVStack({.Spacing = 4.0f, .Padding = 12.0f, .Width = Size::Fill()});
            Text("Release notes", {.Style = TextStyle::Headline});
            Text("From the Carbon team", {.Style = TextStyle::Subheadline, .Secondary = true});
            EndVStack();
            SplitViewDivider();
            BeginVStack({.Padding = 12.0f, .Width = Size::Fill()});
            Text("Drag the dividers. Each pane keeps a minimum size, and the split remembers where you left it.",
                 {.Secondary = true, .Width = Size::Fill(), .Wraps = true});
            EndVStack();
            EndSplitView();
        }
        EndSplitView();
        EndVStack();
        EndSection();
    }

    void ChartsPage(GalleryState& state)
    {
        BeginSection("Line chart", "Change over time. Move the pointer over the chart to read values.");
        const ChartSeries lines[] = {{.Label = "Revenue", .Values = Revenue}, {.Label = "Costs", .Values = Costs}};
        LineChart("revenue", lines, {.Height = 200.0f, .Labels = Months, .ShowsPoints = state.ShowsPoints});
        Toggle("Show points", &state.ShowsPoints, {.Kind = ToggleKind::Checkbox});
        EndSection();

        BeginSection("Bar chart", "Comparing categories. Several series are grouped side by side.");
        const ChartSeries bars[] = {{.Label = "Desktop", .Values = Desktop},
                                    {.Label = "Mobile", .Values = Mobile},
                                    {.Label = "Tablet", .Values = Tablet}};
        BarChart("devices", bars, {.Height = 200.0f, .Labels = Quarters});
        EndSection();
    }
} // namespace Gallery
