#pragma once

#include <cstdint>
#include <optional>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// The two shapes of a progress indicator.
    enum class ProgressKind : uint8_t
    {
        /// A horizontal bar that fills from the leading edge.
        Bar,
        /// A small circular indicator: spinning spokes while indeterminate, a ring that closes otherwise.
        Spinner
    };

    /// Per-call options of ProgressIndicator. All fields are optional.
    struct ProgressIndicatorOptions
    {
        ProgressKind Kind = ProgressKind::Bar;
        /// The duration is unknown: the indicator shows activity instead of an amount, and `value` is ignored.
        bool IsIndeterminate = false;
        /// Length of a bar. Spinners have a fixed size and ignore it.
        Size Width = Size::Fixed(180.0f);
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// Replaces the accent color.
        std::optional<Color> Tint = {};
    };

    /// Shows how far a task has come; `value` goes from 0 to 1. Indeterminate indicators keep animating, so
    /// IsAnimating() stays true while one is shown.
    void ProgressIndicator(float value, const ProgressIndicatorOptions& options = {});
} // namespace Carbon
