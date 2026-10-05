#pragma once

#include <cstdint>

#include "Carbon/Widgets/ControlSize.h"

namespace Carbon
{
    /// Where Reflect puts the label of each field.
    enum class ReflectLayout : uint8_t
    {
        /// Labels in a column on the leading side, as wide as the widest label of the struct; controls after them.
        LabelLeading,
        /// Each label above its control.
        LabelAbove
    };

    /// How Reflect shows an enum: a reflected enum, and every enum field of a reflected struct.
    enum class ReflectEnumStyle : uint8_t
    {
        PopUpButton,
        SegmentedControl,
        RadioGroup
    };

    /// Per-call options of Reflect. All fields are optional.
    struct ReflectOptions
    {
        /// The control for enums. A field's own CB_FIELD Control wins over it.
        ReflectEnumStyle EnumStyle = ReflectEnumStyle::PopUpButton;
        /// Where the labels of a struct's fields go.
        ReflectLayout Layout = ReflectLayout::LabelLeading;
        /// The size of every control.
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        /// Shows everything without letting it be changed.
        bool Disabled = false;
    };
} // namespace Carbon
