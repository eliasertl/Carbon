#pragma once

#include <string_view>

#include <Carbon/Extension.h>

#include "WindowControls.h"

namespace Example
{
    /// Height of the title bar in points: room for toolbar controls of the regular size.
    inline constexpr float TitleBarHeight = 40.0f;

    /// Per-call options of BeginTitleBar.
    struct TitleBarOptions
    {
        std::string_view Title = {};
        /// An icon before the title, such as Carbon::Icons::Notepad.
        std::string_view Icon = {};
        /// The window is maximized: the middle button shows "restore".
        bool IsMaximized = false;
        /// The window has focus. An inactive window's title bar is dimmed.
        bool IsActive = true;
    };

    /// What the user did in the title bar this frame. The host acts on it (see WindowControls).
    struct TitleBarActions
    {
        /// The empty part of the bar was pressed: start moving the window.
        bool BeginMove = false;
        /// The empty part of the bar was double-clicked: maximize or restore.
        bool ToggleMaximize = false;
        bool Minimize = false;
        bool Close = false;
    };

    /// A title bar drawn by Carbon for a window without the system's one, with caption buttons in the style of
    /// the platform: minimize, maximize and close at the trailing edge. Items added between BeginTitleBar and
    /// EndTitleBar form a toolbar after the title, laid out like in an HStack; wherever the bar is empty, it
    /// moves the window.
    ///
    ///     Example::BeginTitleBar({ .Title = "Notes", .IsMaximized = window.IsMaximized() });
    ///     Carbon::SearchField("Search", &query);
    ///     const Example::TitleBarActions actions = Example::EndTitleBar();
    ///
    /// Call it first, at the top of the window.
    void BeginTitleBar(const TitleBarOptions& options);
    TitleBarActions EndTitleBar();

    /// Invisible handles along the window's edges and corners that resize it. Call it last in the frame, so that
    /// the handles win over whatever lies beneath them. `activeEdges` are the edges a resize in progress moves;
    /// the pointer keeps their cursor while it is dragged. Returns the edges to start resizing at, or None.
    WindowEdge WindowResizeHandles(bool isEnabled, WindowEdge activeEdges);
} // namespace Example
