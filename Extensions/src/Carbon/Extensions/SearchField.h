#pragma once

#include <string>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of SearchField. All fields are optional.
    struct SearchFieldOptions
    {
        /// Shown while the field is empty.
        std::string_view Placeholder = "Search";
        Size Width = Size::Fixed(180.0f);
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// A text field for search terms: a magnifying glass at the leading edge, a clear button once there is text,
    /// and Escape empties it. Returns true on frames the text changed; IsItemSubmitted() tells when Enter was
    /// pressed.
    ///
    /// The label identifies the field and is not drawn.
    bool SearchField(std::string_view label, std::string* text, const SearchFieldOptions& options = {});
} // namespace Carbon
