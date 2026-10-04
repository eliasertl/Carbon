#pragma once

#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of Stepper. All fields are optional.
    struct StepperOptions
    {
        /// The value stays within [Min, Max].
        double Min = 0.0;
        double Max = 100.0;
        /// How much one click or arrow key press changes the value.
        double Step = 1.0;
        /// Stepping past one end continues at the other.
        bool Wraps = false;
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// A pair of small buttons that increase and decrease `value` by a fixed step. Holding a button repeats, and
    /// the up and down arrow keys work while the stepper has focus. Returns true on frames the value changed.
    ///
    /// A stepper does not show its value: put a Text or TextField next to it. The label identifies the stepper
    /// and is not drawn.
    bool Stepper(std::string_view label, double* value, const StepperOptions& options = {});
    bool Stepper(std::string_view label, int* value, const StepperOptions& options = {});
} // namespace Carbon
