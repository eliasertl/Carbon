#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Layout/Size.h"
#include "Carbon/Widgets/ControlSize.h"

namespace Carbon
{
    /// Options of Slider. All fields are optional.
    struct SliderOptions
    {
        /// Distance between allowed values; 0 makes the slider continuous. Also the arrow-key increment.
        float Step = 0.0f;
        /// Draws a tick mark at every step.
        bool ShowsTicks = false;
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// Length of the slider. Sliders do not fit their content, so the default is a fixed 180 points.
        Size Width = Size::Fixed(180.0f);
        bool Disabled = false;
        /// Replaces the accent color of the filled part of the track.
        std::optional<Color> Tint = {};
    };

    /// A horizontal slider bound to `value`, which stays within [min, max]. Drag the knob, click the track, or
    /// use the arrow keys, Home and End while focused. Returns true on frames the value changed.
    ///
    /// The label identifies the slider and is not drawn; put a Text next to it for a visible title.
    bool Slider(std::string_view label, float* value, float min, float max, const SliderOptions& options = {});
} // namespace Carbon
