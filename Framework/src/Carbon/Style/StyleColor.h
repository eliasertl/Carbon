#pragma once

#include <cstdint>

namespace Carbon
{
    /// The semantic colors of a theme, modeled on the macOS system colors. Widgets ask for a role ("label",
    /// "separator"), never for a concrete color, so they follow the theme and the style stack.
    enum class StyleColor : uint8_t
    {
        /// The base surface of the interface: white in the light theme, pure black in the dark theme.
        Background,
        /// Grouped content on top of the background: boxes, sidebars, cards.
        SecondaryBackground,
        /// Content layered on a secondary background, and alternating table rows.
        TertiaryBackground,

        /// Primary text and icons.
        Label,
        /// Secondary text: subtitles, captions, explanations.
        SecondaryLabel,
        /// Tertiary text: placeholders and disabled labels.
        TertiaryLabel,
        /// The faintest text and decoration.
        QuaternaryLabel,
        /// Thin lines between content.
        Separator,

        /// The inside of editable controls such as text fields.
        ControlBackground,
        /// The fill of buttons, slider tracks and switches that are off. A translucent gray, so it adapts to the
        /// surface it sits on.
        ControlFill,
        /// The outline of bordered controls.
        ControlBorder,
        /// The knob of switches and sliders.
        Knob,

        /// The accent color: prominent buttons, switches that are on, selection, links.
        Accent,
        /// Text and icons on top of the accent color.
        OnAccent,
        /// The background of selected rows in a focused list.
        Selection,
        /// The background of selected rows in a list that does not have focus.
        UnemphasizedSelection,
        /// The background of selected text.
        TextSelection,
        /// Destructive actions and errors.
        Destructive,
        /// The keyboard focus ring.
        FocusRing,

        /// The surface of menus, popovers, alerts and sheets.
        OverlayBackground,
        /// The hairline around overlays.
        OverlayBorder,
        /// The dimming layer behind modal overlays.
        Scrim,
        /// Shadows under overlays and knobs.
        Shadow,
        /// Overlay scroll indicators.
        ScrollIndicator,

        /// The system palette, for charts, tags and status colors.
        Red,
        Orange,
        Yellow,
        Green,
        Mint,
        Teal,
        Cyan,
        Blue,
        Indigo,
        Purple,
        Pink,
        Brown,
        Gray,

        Count
    };
} // namespace Carbon
