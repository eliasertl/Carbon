#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace Carbon
{
    /// The control Reflect draws for a field. Automatic picks one from the field's type (see Docs/Reflection.md);
    /// the others must suit the type, which is checked when the program is compiled.
    enum class ReflectControl : uint8_t
    {
        Automatic,
        /// A bool as a switch (the automatic choice for bool).
        Switch,
        /// A bool as a checkbox.
        Checkbox,
        /// A number between Min and Max (both required).
        Slider,
        /// A number that steps by Step, shown next to its value.
        Stepper,
        /// An enum as a pop-up button.
        PopUpButton,
        /// An enum as a segmented control.
        SegmentedControl,
        /// An enum as a group of radio buttons.
        RadioGroup
    };

    /// Metadata of one field of a reflected struct, given with CB_FIELD. Every field is optional.
    struct ReflectFieldOptions
    {
        /// The label; empty derives it from the field's name ("DarkMode" becomes "Dark Mode").
        std::string_view DisplayName = {};
        /// Lower end of a number's range. With Max, a number becomes a slider.
        std::optional<double> Min = {};
        /// Upper end of a number's range.
        std::optional<double> Max = {};
        /// Increment of a stepper or slider; empty uses 1 for integers and 0.1 for floating-point numbers.
        std::optional<double> Step = {};
        /// Overrides the control chosen from the field's type.
        ReflectControl Control = ReflectControl::Automatic;
        /// Help text shown when the pointer rests on the field's row.
        std::string_view Tooltip = {};
        /// Leaves the field out of the interface.
        bool Hidden = false;
        /// Shows the field without letting it be changed.
        bool ReadOnly = false;
    };
} // namespace Carbon
