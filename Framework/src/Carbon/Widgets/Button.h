#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Layout/Size.h"
#include "Carbon/Widgets/ControlSize.h"

namespace Carbon
{
    /// What a button means, which decides how it looks.
    enum class ButtonRole : uint8_t
    {
        /// A standard bordered button.
        Default,
        /// The most likely action of a view: filled with the accent color. Use one or two per view.
        Prominent,
        /// No background until hovered: for toolbars and inline actions.
        Plain,
        /// An action that destroys data: red label.
        Destructive
    };

    /// Options of Button. All fields are optional.
    struct ButtonOptions
    {
        ButtonRole Role = ButtonRole::Default;
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// An icon from Carbon::Icons shown before the label. With an empty label the button shows only the icon.
        std::string_view Icon = {};
        /// Fit hugs the label; Fixed and Fill stretch the button and center the label.
        Size Width = Size::Fit();
        bool Disabled = false;
        /// The default button of a dialog: also activates with Enter when no focused control uses the key.
        bool IsDefault = false;
        /// Corner radius; from the theme and the control size when not set.
        std::optional<float> CornerRadius = {};
        /// Corner smoothing; the theme's CornerSmoothing when not set.
        std::optional<float> CornerSmoothing = {};
        /// Replaces the accent color of a Prominent or Plain button.
        std::optional<Color> Tint = {};
    };

    /// A push button. Returns true on the frame it is activated: clicked, or Space/Enter while focused.
    /// The label is also the button's ID; use "Label##suffix" to tell apart buttons with the same label.
    bool Button(std::string_view label, const ButtonOptions& options = {});
} // namespace Carbon
