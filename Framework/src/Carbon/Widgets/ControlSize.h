#pragma once

#include <cstdint>

#include "Carbon/Text/TextStyle.h"

namespace Carbon
{
    /// The size of a control, as on macOS. Regular is the default; small suits dense inspectors and large suits
    /// the main action of a dialog.
    enum class ControlSize : uint8_t
    {
        Small,
        Regular,
        Large
    };

    /// The measurements controls of one size share.
    struct ControlMetrics
    {
        /// Height of buttons, fields and other single-line controls.
        float Height = 24.0f;
        /// Horizontal padding between a control's edge and its content.
        float Padding = 10.0f;
        float CornerRadius = 6.0f;
        /// The text style of the control's label.
        TextStyle Style = TextStyle::Body;
    };

    /// Returns the measurements for a control size, derived from the current theme's ControlHeight,
    /// ControlPadding and CornerRadius.
    ControlMetrics GetControlMetrics(ControlSize size);
} // namespace Carbon
