#pragma once

#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of RadioGroup. All fields are optional.
    struct RadioGroupOptions
    {
        /// Vertical stacks the buttons; Horizontal puts them in a row, each as wide as the widest.
        Axis Orientation = Axis::Vertical;
        /// Distance between buttons: 6 points vertically, 20 horizontally when not set.
        std::optional<float> Spacing = {};
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// A set of radio buttons of which at most one is selected; `selected` is its index, -1 for none. Click a
    /// button, or use the arrow keys while the group has focus. Returns true on frames the selection changed.
    ///
    /// The label identifies the group and is not drawn: put a title in front of the group, as in a form.
    bool RadioGroup(std::string_view label, int* selected, std::span<const std::string_view> items,
                    const RadioGroupOptions& options = {});
    bool RadioGroup(std::string_view label, int* selected, std::initializer_list<std::string_view> items,
                    const RadioGroupOptions& options = {});
} // namespace Carbon
