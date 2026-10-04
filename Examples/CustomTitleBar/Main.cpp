// CustomTitleBar: a window without the system's title bar, whose header is drawn by Carbon. The title bar holds the
// app's toolbar and caption buttons in the platform's style; dragging it moves the window, double-clicking it
// maximizes, and the window's edges resize it.
//
// Carbon draws and reports, the host acts: TitleBar.cpp is Carbon code, WindowControls.cpp is the window side.
//
//   CustomTitleBar [--theme light|dark] [--scale <factor>] [--screenshot <file.png>]

#include <format>
#include <string>

#include <Carbon/Extensions/Extensions.h>

#include "ExampleApp.h"
#include "TitleBar.h"
#include "WindowControls.h"

namespace
{
    using namespace Carbon;

    struct Note
    {
        const char* Title;
        const char* Date;
        const char* Body;
    };

    const Note Notes[] = {
        {"Release checklist", "Today", "Update the screenshots, run the tests on every platform, tag the release."},
        {"Ideas for the sidebar", "Yesterday", "Badges for unread counts. Collapsible groups. Drag to reorder."},
        {"Groceries", "Monday", "Coffee, oranges, bread, a new notebook."},
        {"Meeting notes", "Last week", "Agreed to ship the title bar example. Window chrome stays in the host."},
    };

    struct NotesState
    {
        int Folder = 0;
        int Selected = 0;
        std::string Query;
        bool IsDark = false;
        std::string LastAction = "None yet";
    };

    void BuildWindow(NotesState& state, Example::WindowControls& window)
    {
        window.Update();

        BeginVStack({.Spacing = 0.0f, .Width = Size::Fill(), .Height = Size::Fill()});

        // ---- The title bar, with the app's toolbar in it.
        Example::BeginTitleBar({.Title = "Notes",
                                .Icon = Icons::Notepad,
                                .IsMaximized = window.IsMaximized(),
                                .IsActive = window.IsActive()});
        if (BeginPullDownButton("New", {.Icon = Icons::Plus, .ControlSize = ControlSize::Small}))
        {
            if (MenuItem("Note", {.Icon = Icons::NotePencil, .Shortcut = "Ctrl+N"}))
                state.LastAction = "New note";
            if (MenuItem("Folder", {.Icon = Icons::Folder}))
                state.LastAction = "New folder";
            EndPullDownButton();
        }
        Spacer();
        SearchField("Search", &state.Query,
                    {.Placeholder = "Search notes", .Width = 220.0f, .ControlSize = ControlSize::Small});
        Spacer();
        if (Toggle("Dark", &state.IsDark, {.ControlSize = ControlSize::Small}))
            SetTheme(state.IsDark ? Theme::Dark() : Theme::Light());
        const Example::TitleBarActions actions = Example::EndTitleBar();

        // ---- The window's content.
        BeginHStack(
            {.Spacing = 0.0f, .Alignment = VerticalAlignment::Top, .Width = Size::Fill(), .Height = Size::Fill()});
        BeginSidebar("folders", {.Width = 200.0f});
        SidebarHeader("Library");
        if (SidebarItem("All Notes", state.Folder == 0, {.Icon = Icons::Tray, .Badge = "4"}))
            state.Folder = 0;
        if (SidebarItem("Pinned", state.Folder == 1, {.Icon = Icons::PushPin, .Badge = "1"}))
            state.Folder = 1;
        if (SidebarItem("Recent", state.Folder == 2, {.Icon = Icons::Clock}))
            state.Folder = 2;
        SidebarHeader("Other");
        if (SidebarItem("Archive", state.Folder == 3, {.Icon = Icons::Archive}))
            state.Folder = 3;
        if (SidebarItem("Trash", state.Folder == 4, {.Icon = Icons::Trash}))
            state.Folder = 4;
        EndSidebar();

        BeginScrollView("note", {.Spacing = 14.0f, .Padding = 24.0f});
        BeginList("notes", {.Height = 4.0f * 24.0f + 10.0f});
        for (int i = 0; i < static_cast<int>(std::size(Notes)); i++)
        {
            PushID(i);
            if (ListItem(Notes[i].Title, i == state.Selected, {.Icon = Icons::Note, .Detail = Notes[i].Date}))
                state.Selected = i;
            PopID();
        }
        EndList();

        const Note& note = Notes[state.Selected];
        Text(note.Title, {.Style = TextStyle::Title2, .Emphasized = true});
        Text(note.Date, {.Style = TextStyle::Subheadline, .Secondary = true});
        Text(note.Body, {.Width = Size::Fill(), .Wraps = true});
        Separator();
        Text(
            "This window has no system title bar. Drag the bar to move the window, double-click it to maximize, "
            "and drag the edges to resize. The caption buttons at the trailing edge minimize, maximize and close.",
            {.Style = TextStyle::Subheadline, .Secondary = true, .Width = Size::Fill(), .Wraps = true});
        Text(std::format("Last command: {}", state.LastAction), {.Style = TextStyle::Subheadline, .Secondary = true});
        EndScrollView();
        EndHStack();

        EndVStack();

        // ---- The window's frame: a hairline border while the window is not maximized, and the resize handles,
        // submitted last so that they win the pointer at the edges.
        if (!window.IsMaximized())
        {
            GetDrawList().AddSquircleStroke(Rect(Vec2(), GetDisplaySize()), GetStyleColor(StyleColor::ControlBorder),
                                            0.0f, GetContentScale().GetPixelSize(), 0.0f);
        }
        const Example::WindowEdge resizeAt =
            Example::WindowResizeHandles(!window.IsMaximized() && !window.IsMoving(), window.GetResizeEdges());

        // ---- The host acts on what the user did.
        if (actions.BeginMove)
            window.BeginMove();
        if (actions.ToggleMaximize)
            window.ToggleMaximize();
        if (actions.Minimize)
            window.Minimize();
        if (actions.Close)
            window.Close();
        if (resizeAt != Example::WindowEdge::None)
            window.BeginResize(resizeAt);
    }
} // namespace

int main(int argc, char** argv)
{
    Example::App app(argc, argv, "Notes", 900, 600, true);
    if (!app.IsReady())
        return 1;

    NotesState state;
    state.IsDark = app.GetArguments().IsDark;
    Example::WindowControls window(app.GetHost().GetWindow(), 560.0f, 360.0f);
    return app.Run([&] { BuildWindow(state, window); });
}
