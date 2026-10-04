#pragma once

#include <initializer_list>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of PopUpButton. All fields are optional.
    struct PopUpButtonOptions
    {
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// Fit makes the button as wide as its widest item.
        Size Width = Size::Fit();
        bool Disabled = false;
    };

    /// A button that shows the selected one of several mutually exclusive items and opens a menu to choose
    /// another; `selected` is the index. The menu opens over the button with the current item under the
    /// pointer. Returns true on frames the selection changed.
    ///
    /// The label identifies the button and is not drawn.
    bool PopUpButton(std::string_view label, int* selected, std::span<const std::string_view> items,
                     const PopUpButtonOptions& options = {});
    bool PopUpButton(std::string_view label, int* selected, std::initializer_list<std::string_view> items,
                     const PopUpButtonOptions& options = {});
} // namespace Carbon
