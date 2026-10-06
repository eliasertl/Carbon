#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Layout/Size.h"
#include "Carbon/Widgets/ControlSize.h"

namespace Carbon
{
    /// How the position of a slider's knob maps to its value.
    enum class SliderScale : uint8_t
    {
        /// Equal distances are equal differences of value.
        Linear,
        /// Equal distances are equal ratios of value, for ranges over several orders of magnitude (frequencies,
        /// zoom factors). The range must be positive.
        Logarithmic
    };

    /// Options of Slider. All fields are optional.
    struct SliderOptions
    {
        /// Distance between allowed values; 0 makes the slider continuous (an int slider then steps by 1). Also the
        /// arrow-key increment.
        double Step = 0.0;
        /// Draws a tick mark at every step.
        bool ShowsTicks = false;
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// Length of a horizontal slider. Sliders do not fit their content, so the default is a fixed 180 points.
        Size Width = Size::Fixed(180.0f);
        bool Disabled = false;
        /// Replaces the accent color of the filled part of the track.
        std::optional<Color> Tint = {};
        /// The direction the slider runs in: leading to trailing, or bottom to top when vertical.
        Carbon::Axis Axis = Carbon::Axis::Horizontal;
        /// Length of a vertical slider.
        Size Height = Size::Fixed(180.0f);
        /// How the knob's position maps to the value.
        SliderScale Scale = SliderScale::Linear;
    };

    /// A slider bound to `value`, which stays within [min, max]. Drag the knob, click the track, or use the arrow
    /// keys, Home and End while focused. Returns true on frames the value changed.
    ///
    /// The label identifies the slider and is not drawn; put a Text next to it for a visible title. An empty
    /// range, and a logarithmic scale whose range is not positive, are reported and the slider is not drawn.
    bool Slider(std::string_view label, float* value, float min, float max, const SliderOptions& options = {});
    bool Slider(std::string_view label, double* value, double min, double max, const SliderOptions& options = {});
    /// An int slider moves in whole steps of at least 1.
    bool Slider(std::string_view label, int* value, int min, int max, const SliderOptions& options = {});
} // namespace Carbon
