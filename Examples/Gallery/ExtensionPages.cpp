// The gallery's pages for the components of CarbonExtensions.

#include <array>
#include <format>
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
