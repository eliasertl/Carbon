#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Layout/Size.h"
#include "Carbon/Widgets/ControlSize.h"

namespace Carbon
{
    /// How a toggle looks.
    enum class ToggleKind : uint8_t
    {
        /// A switch: for settings that deserve visual weight.
        Switch,
        /// A checkbox: for lists and hierarchies of options.
        Checkbox
    };

    /// Options of Toggle. All fields are optional.
    struct ToggleOptions
    {
        ToggleKind Kind = ToggleKind::Switch;
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// Fit hugs label and control. With Fixed or Fill, a switch moves to the trailing edge, like a row in
        /// System Settings.
        Size Width = Size::Fit();
        bool Disabled = false;
        /// Checkbox only: shows a dash instead of a checkmark, for a parent whose children differ.
        bool IsMixed = false;
        /// Replaces the accent color of the "on" state.
        std::optional<Color> Tint = {};
    };

    /// A switch or checkbox bound to `value`. Clicking anywhere on the row (label included) or pressing Space
    /// while focused flips it. Returns true on the frame the value changed.
    bool Toggle(std::string_view label, bool* value, const ToggleOptions& options = {});
} // namespace Carbon
