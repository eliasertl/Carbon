#pragma once

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of ComboBox. All fields are optional.
    struct ComboBoxOptions
    {
        /// Shown while the field is empty.
        std::string_view Placeholder = {};
        /// Width of the whole control. Make it wide enough for the items: they are not truncated in the field.
        Size Width = Size::Fixed(180.0f);
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// While the user types, the list shows only the items that contain the text, and opens by itself.
        bool FiltersWhileTyping = true;
        /// Longest text the user can enter, in characters; 0 means unlimited.
        size_t MaxLength = 0;
        bool Disabled = false;
    };

    /// A text field combined with a list of choices: type any value, or pick one of the items from the list that
    /// the button at the trailing edge opens. A typed value is not added to the items. Returns true on frames the
    /// text changed, by typing or by picking; IsItemSubmitted() tells when Enter was pressed.
    ///
    ///     std::string font = "Helvetica";
    ///     Carbon::ComboBox("Font", &font, { "Courier", "Helvetica", "Times" });
    ///
    /// While the field has focus, the up and down arrow keys move through the list, Enter picks the highlighted
    /// item and Escape closes the list. The label identifies the control and is not drawn; put a Text with a colon
    /// before it, as the HIG recommends.
    bool ComboBox(std::string_view label, std::string* text, std::span<const std::string_view> items,
                  const ComboBoxOptions& options = {});
    bool ComboBox(std::string_view label, std::string* text, std::initializer_list<std::string_view> items,
                  const ComboBoxOptions& options = {});
} // namespace Carbon
