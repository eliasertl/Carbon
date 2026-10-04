#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginSidebar. All fields are optional.
    struct SidebarOptions
    {
        float Width = 220.0f;
        EdgeInsets Padding = EdgeInsets(10.0f);
    };

    /// Per-call options of SidebarItem. All fields are optional.
    struct SidebarItemOptions
    {
        /// An icon before the title, such as Carbon::Icons::Folder.
        std::string_view Icon = {};
        /// Short text at the trailing edge, typically a count.
        std::string_view Badge = {};
        /// Replaces the accent color of the icon.
        std::optional<Color> IconTint = {};
        bool Disabled = false;
    };

    /// A sidebar is the column at the leading edge of a window that navigates between the areas of an app. It
    /// fills the height of its container and scrolls when its items do not fit. Put it first in an HStack that
    /// fills the window, followed by the content.
    ///
    ///     Carbon::BeginSidebar("sidebar");
    ///     Carbon::SidebarHeader("Library");
    ///     if (Carbon::SidebarItem("Songs", page == Page::Songs, { .Icon = Carbon::Icons::MusicNotes }))
    ///         page = Page::Songs;
    ///     Carbon::EndSidebar();
    ///
    /// The sidebar is one stop for Tab; with focus, the up and down arrow keys move the selection.
    void BeginSidebar(std::string_view id, const SidebarOptions& options = {});
    void EndSidebar();

    /// The title of a group of items.
    void SidebarHeader(std::string_view title);

    /// One destination. Pass whether it is the selected one; the highlight slides to it. Returns true when the
    /// user picks it, by click or keyboard: make it the selection.
    bool SidebarItem(std::string_view label, bool isSelected, const SidebarItemOptions& options = {});
} // namespace Carbon
