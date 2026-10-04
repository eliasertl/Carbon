#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginPopover. All fields are optional.
    struct PopoverOptions
    {
        /// The rectangle the popover points at. Defaults to the item submitted last, usually the button that
        /// opens it.
        std::optional<Rect> Anchor = {};
        /// The side of the anchor the popover appears on. It flips over when there is no room.
        OverlayPlacement Placement = OverlayPlacement::Below;
        Size Width = Size::Fit();
        Size Height = Size::Fit();
        EdgeInsets Padding = EdgeInsets(16.0f);
        /// Distance between items. Defaults to the theme's Spacing.
        std::optional<float> Spacing = {};
        bool ShowsArrow = true;
        /// A click outside closes the popover. Turn it off for a popover that stays while the user works
        /// elsewhere; it then needs its own way to close.
        bool DismissOnOutsideClick = true;
    };

    /// Opens the popover `id`. Call it at the same ID scope as BeginPopover.
    void OpenPopover(std::string_view id);
    /// Closes the popover `id` and anything opened from it.
    void ClosePopover(std::string_view id);
    bool IsPopoverOpen(std::string_view id);

    /// A popover is a transient view that appears next to the control it belongs to and points at it with an
    /// arrow. Returns true while it is open; then add its content, laid out like in a VStack, and call
    /// EndPopover.
    ///
    ///     if (Carbon::Button("Info"))
    ///         Carbon::OpenPopover("info");
    ///     if (Carbon::BeginPopover("info"))
    ///     {
    ///         Carbon::Text("Details");
    ///         Carbon::EndPopover();
    ///     }
    bool BeginPopover(std::string_view id, const PopoverOptions& options = {});
    void EndPopover();
} // namespace Carbon
