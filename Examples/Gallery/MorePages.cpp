// The gallery's pages for hierarchies (outline and column views) and notifications.

#include <format>
#include <span>
#include <string>

#include "Pages.h"

namespace Gallery
{
    using namespace Carbon;

    namespace
    {
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
                options.Image = GetTextureID(state.Artwork);
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
} // namespace Gallery
