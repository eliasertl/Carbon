#pragma once

#include <cstdint>
#include <string_view>

#include "Carbon/Extensions/NumberField.h"

namespace Carbon::Internal
{
    /// The types a number control can be bound to; each rounds the value in its own way.
    enum class NumberKind : uint8_t
    {
        Int,
        Float,
        Double
    };

    /// What NumberField and ScrubField have in common.
    struct NumberEditOptions
    {
        double Min = 0.0;
        double Max = 0.0;
        double Step = 1.0;
        NumberFormat Format = {};
        Size Width = Size::Fit();
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
        /// Shows the value for dragging until it is clicked or tabbed to (ScrubField); otherwise a text field.
        bool Scrubs = false;
    };

    /// The control behind NumberField and ScrubField, working on the value as a double. Returns true when the
    /// value changed.
    bool EditNumber(std::string_view label, double& value, NumberKind kind, const NumberEditOptions& options);

    /// Converts a bound value to a double and back around EditNumber.
    template <typename T>
    bool EditNumberValue(std::string_view label, T* value, NumberKind kind, const NumberEditOptions& options)
    {
        CB_VERIFY(value != nullptr, "A number control needs a value to bind to");
        if (value == nullptr)
            return false;
        double edited = static_cast<double>(*value);
        if (!EditNumber(label, edited, kind, options))
            return false;
        const T result = static_cast<T>(edited);
        const bool changed = result != *value;
        *value = result;
        return changed;
    }
} // namespace Carbon::Internal
