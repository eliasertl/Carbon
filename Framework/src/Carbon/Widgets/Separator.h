#pragma once

#include <optional>

#include "Carbon/Core/Color.h"

namespace Carbon
{
    /// Options of Separator. All fields are optional.
    struct SeparatorOptions
    {
        /// The line color; the theme's Separator when not set.
        std::optional<Carbon::Color> Color = {};
        /// The line thickness; the theme's BorderWidth when not set.
        std::optional<float> Thickness = {};
    };

    /// Draws a thin line between items: horizontal in a vertical stack, vertical in a horizontal stack. It spans
    /// the stack across its axis.
    void Separator(const SeparatorOptions& options = {});
} // namespace Carbon
