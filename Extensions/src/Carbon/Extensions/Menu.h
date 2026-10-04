#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginMenu. All fields are optional.
    struct MenuOptions
    {
        /// The rectangle the menu belongs to. Defaults to the item submitted last, usually the button that
        /// opens it.
        std::optional<Rect> Anchor = {};
        OverlayPlacement Placement = OverlayPlacement::Below;
        /// Which edge of the anchor the menu lines up with.
        Carbon::Alignment Alignment = Carbon::Alignment::Leading;
        float Gap = 2.0f;
        /// The menu is at least this wide, for example as wide as its button.
        float MinWidth = 0.0f;
    };

    /// Per-call options of MenuItem and BeginSubmenu. All fields are optional.
    struct MenuItemOptions
    {
        /// An icon in the leading column, such as Carbon::Icons::Copy.
        std::string_view Icon = {};
        /// The keyboard shortcut, shown at the trailing edge. It is only a hint: the menu does not listen for
        /// it. Use IsShortcutPressed where the command lives.
        std::string_view Shortcut = {};
        /// Shows a checkmark in the leading column.
        bool IsChecked = false;
        bool Disabled = false;
        /// Draws the title in the destructive color.
        bool IsDestructive = false;
    };

    /// Opens the menu `id`. Call it at the same ID scope as BeginMenu.
    void OpenMenu(std::string_view id);
    bool IsMenuOpen(std::string_view id);

    /// A menu is a list of commands that appears on demand. Returns true while it is open; then add items and
    /// call EndMenu.
    ///
    ///     if (Carbon::Button("Edit"))
    ///         Carbon::OpenMenu("edit");
    ///     if (Carbon::BeginMenu("edit"))
    ///     {
    ///         if (Carbon::MenuItem("Copy", { .Shortcut = "Ctrl+C" }))
    ///             Copy();
    ///         Carbon::EndMenu();
    ///     }
    ///
    /// The up and down arrow keys move the highlight, Enter and Space choose, the right and left arrow keys
    /// open and close submenus, Escape closes the menu.
    bool BeginMenu(std::string_view id, const MenuOptions& options = {});
    /// The same for a menu opened with OpenOverlay(id). Components that own a menu use this overload.
    bool BeginMenu(ID id, const MenuOptions& options = {});
    void EndMenu();

    /// A command. Returns true when it is chosen, which also closes the menu.
    bool MenuItem(std::string_view label, const MenuItemOptions& options = {});
    /// A line that separates groups of items.
    void MenuSeparator();
    /// A small title above a group of items.
    void MenuHeader(std::string_view title);

    /// An item that opens another menu next to this one. Returns true while that menu is open; then add its
    /// items and call EndSubmenu.
    bool BeginSubmenu(std::string_view label, const MenuItemOptions& options = {});
    void EndSubmenu();

    /// A menu that opens at the pointer when the item submitted last is clicked with the right mouse button.
    /// Returns true while it is open; then add items and call EndContextMenu.
    ///
    ///     Carbon::Text("report.pdf");
    ///     if (Carbon::BeginContextMenu("file"))
    ///     {
    ///         Carbon::MenuItem("Rename");
    ///         Carbon::EndContextMenu();
    ///     }
    bool BeginContextMenu(std::string_view id);
    void EndContextMenu();
} // namespace Carbon
