#pragma once

#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginPullDownButton. All fields are optional.
    struct PullDownButtonOptions
    {
        /// An icon before the title, such as Carbon::Icons::Plus.
        std::string_view Icon = {};
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        Size Width = Size::Fit();
        bool Disabled = false;
    };

    /// A button that opens a menu of commands below it. Unlike a pop-up button it keeps its title: the items
    /// are things to do, not a value to pick. Returns true while the menu is open; then add items with MenuItem
    /// (see Menu.h) and call EndPullDownButton.
    ///
    ///     if (Carbon::BeginPullDownButton("Add"))
    ///     {
    ///         if (Carbon::MenuItem("Folder")) ...
    ///         Carbon::EndPullDownButton();
    ///     }
    bool BeginPullDownButton(std::string_view label, const PullDownButtonOptions& options = {});
    void EndPullDownButton();
} // namespace Carbon
