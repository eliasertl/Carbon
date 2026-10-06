#pragma once

#include <limits>
#include <string_view>

#include "Carbon/Extensions/NumberField.h"

namespace Carbon
{
    /// Per-call options of ScrubField. All fields are optional.
    struct ScrubFieldOptions
    {
        /// The value stays within [Min, Max]; unlimited by default.
        double Min = -std::numeric_limits<double>::infinity();
        double Max = std::numeric_limits<double>::infinity();
        /// How much the value changes per point the pointer is dragged, and per press of an arrow key while typing.
        /// Holding Shift changes it by a tenth of this, holding Ctrl by ten times as much. Values from dragging
        /// are multiples of that amount.
        double Step = 1.0;
        /// How the value is shown.
        NumberFormat Format = {};
        Size Width = Size::Fixed(80.0f);
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// A number that is changed by dragging: drag horizontally to scrub the value, with Shift for fine and Ctrl for
    /// coarse steps. A click without dragging turns it into a number field with the value selected for typing,
    /// and so does Tab; Enter or leaving the field applies the typed value, Escape discards it. Returns true on
    /// frames the value changed.
    ///
    /// The label identifies the control and is not drawn.
    bool ScrubField(std::string_view label, int* value, const ScrubFieldOptions& options = {});
    bool ScrubField(std::string_view label, float* value, const ScrubFieldOptions& options = {});
    bool ScrubField(std::string_view label, double* value, const ScrubFieldOptions& options = {});
} // namespace Carbon
