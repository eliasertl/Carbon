#pragma once

#include <optional>
#include <string_view>

// A custom component needs nothing but Carbon's public extension API.
#include <Carbon/Extension.h>

namespace Example
{
    /// Per-call options of StarRating, in the style of Carbon's own components: a plain struct whose fields all
    /// have defaults, so that callers name only what they change.
    struct StarRatingOptions
    {
        /// Number of stars.
        int Count = 5;
        /// Edge length of one star in points.
        float StarSize = 20.0f;
        /// Replaces the theme's yellow.
        std::optional<Carbon::Color> Tint = {};
        bool Disabled = false;
    };

    /// A row of stars bound to `rating` (0 to Count). Click a star to set the rating, click the current rating
    /// again to clear it, or use the left and right arrow keys while the control has focus. Returns true on
    /// frames the rating changed.
    ///
    /// The label identifies the control and is not drawn; text after "##" is part of the ID only.
    bool StarRating(std::string_view label, int* rating, const StarRatingOptions& options = {});
} // namespace Example
