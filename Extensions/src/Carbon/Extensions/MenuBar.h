#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginMenuBar. All fields are optional.
    struct MenuBarOptions
    {
        /// Fill spans the window, as a menu bar below the title bar. Fit makes the bar as wide as its menus, for a
        /// menu bar inside a custom title bar.
        Size Width = Size::Fill();
        float Height = 28.0f;
        /// Defaults to the theme's SecondaryBackground. Pass Color::Transparent() for a bar that sits on
        /// something else, such as a title bar.
        std::optional<Color> Background = {};
        /// A hairline along the bottom edge.
        bool HasSeparator = true;
    };

    /// Per-call options of BeginMenuBarMenu. All fields are optional.
    struct MenuBarMenuOptions
    {
        bool Disabled = false;
    };

    /// A row of menu titles at the top of a window, as on Windows and Linux. A press on a title opens its menu;
    /// while one is open, moving the pointer to another title opens that one instead.
    ///
    ///     Carbon::BeginMenuBar();
    ///     if (Carbon::BeginMenuBarMenu("File"))
    ///     {
    ///         if (Carbon::MenuItem("New", { .Shortcut = "Ctrl+N" }))
    ///             NewDocument();
    ///         Carbon::EndMenuBarMenu();
    ///     }
    ///     if (Carbon::BeginMenuBarMenu("Edit"))
    ///     {
    ///         // ...
    ///         Carbon::EndMenuBarMenu();
    ///     }
    ///     Carbon::EndMenuBar();
    ///
    /// The menus are ordinary menus: add MenuItem, MenuSeparator, MenuHeader and BeginSubmenu (see Menu.h).
    /// Shortcuts shown in items are hints; check them with IsShortcutPressed where the command lives, so that they
    /// work while the menu is closed.
    ///
    /// Keyboard: Alt pressed and released on its own, or F10, opens the first menu. With a menu open, the left
    /// and right arrow keys move to the neighbouring menu; Escape closes it.
    void BeginMenuBar(const MenuBarOptions& options = {});
    void EndMenuBar();

    /// A title of the menu bar. Returns true while its menu is open; then add its items and call EndMenuBarMenu.
    bool BeginMenuBarMenu(std::string_view title, const MenuBarMenuOptions& options = {});
    void EndMenuBarMenu();
} // namespace Carbon
