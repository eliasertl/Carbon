#pragma once

#include <initializer_list>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of SegmentedControl. All fields are optional.
    struct SegmentedControlOptions
    {
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// Fit makes every segment as wide as the widest label; Fixed and Fill divide the given width equally.
        Size Width = Size::Fit();
        bool Disabled = false;
    };

    /// A row of segments of which exactly one is selected; `selected` is its index. Click a segment, or use the
    /// left and right arrow keys while the control has focus. The selection slides to its new place. Returns
    /// true on frames the selection changed.
    ///
    /// The label identifies the control and is not drawn.
    bool SegmentedControl(std::string_view label, int* selected, std::span<const std::string_view> segments,
                          const SegmentedControlOptions& options = {});
    bool SegmentedControl(std::string_view label, int* selected, std::initializer_list<std::string_view> segments,
                          const SegmentedControlOptions& options = {});
} // namespace Carbon
