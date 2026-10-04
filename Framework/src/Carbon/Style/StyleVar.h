#pragma once

#include <cstdint>

namespace Carbon
{
    /// The metrics of a theme. Lengths are in points.
    enum class StyleVar : uint8_t
    {
        /// Corner radius of controls: buttons, fields, selection highlights.
        CornerRadius,
        /// Corner smoothing of every rounded shape: 0 is a circular arc, 1 the maximum.
        CornerSmoothing,
        /// Corner radius of grouped boxes and cards.
        GroupCornerRadius,
        /// Corner radius of menus, popovers, alerts and sheets.
        OverlayCornerRadius,

        /// Default distance between the items of a stack.
        Spacing,
        /// Height of regular-size controls.
        ControlHeight,
        /// Horizontal padding inside buttons and fields.
        ControlPadding,
        /// Width of control outlines and separators.
        BorderWidth,

        /// Width of the keyboard focus ring.
        FocusRingWidth,
        /// Gap between a control and its focus ring.
        FocusRingOffset,

        /// Opacity multiplier of disabled content.
        DisabledOpacity,
        /// How strongly a control's fill shifts towards the label color while hovered (0..1).
        HoverAmount,
        /// How strongly a control's fill shifts towards the label color while pressed (0..1).
        PressedAmount,

        /// Width of overlay scroll indicators.
        ScrollIndicatorWidth,

        Count
    };
} // namespace Carbon
