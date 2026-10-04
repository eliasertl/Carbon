#pragma once

#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of ColorWell. All fields are optional.
    struct ColorWellOptions
    {
        /// Adds a slider for the color's opacity.
        bool ShowsOpacity = false;
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// A swatch that shows `color` and opens a popover to change it: a palette of the theme's colors, sliders
    /// for red, green and blue, and a field for the hex value. Returns true on frames the color changed.
    ///
    /// The label identifies the color well and is not drawn.
    bool ColorWell(std::string_view label, Color* color, const ColorWellOptions& options = {});
} // namespace Carbon
