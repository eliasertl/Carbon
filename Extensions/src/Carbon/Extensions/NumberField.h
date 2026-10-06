#pragma once

#include <limits>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// How a number is written, in a NumberField or ScrubField or by FormatNumber. All fields are optional.
    struct NumberFormat
    {
        /// Digits after the decimal point. -1 writes up to three and drops trailing zeros, so whole numbers have
        /// none.
        int Decimals = -1;
        /// Written before and after the number, e.g. "$" or " px". Typed text may include them or leave them out.
        std::string_view Prefix = {};
        std::string_view Suffix = {};
    };

    /// Writes `value` into `buffer` as `format` says and returns the text, cut off at the end of the buffer.
    /// Does not allocate.
    std::string_view FormatNumber(double value, const NumberFormat& format, std::span<char> buffer);

    /// Reads a number a person typed: spaces around it, the format's prefix and suffix, a sign, digits with '.'
    /// (or a single ',') as the decimal point, and an exponent are accepted. Returns false and leaves `value`
    /// alone when the text is not a finite number.
    bool ParseNumber(std::string_view text, const NumberFormat& format, double* value);

    /// Per-call options of NumberField. All fields are optional.
    struct NumberFieldOptions
    {
        /// The value stays within [Min, Max]; unlimited by default.
        double Min = -std::numeric_limits<double>::infinity();
        double Max = std::numeric_limits<double>::infinity();
        /// How much the up and down arrow keys change the value. With Shift they change it by a tenth of this, with
        /// Ctrl by ten times as much.
        double Step = 1.0;
        /// How the value is shown while it is not being edited.
        NumberFormat Format = {};
        /// Width of the field. Number fields do not fit their content, so the default is a fixed 80 points.
        Size Width = Size::Fixed(80.0f);
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// A text field for a number. It shows `value` formatted; what the user types is applied when they press
    /// Enter or leave the field (Tab, a click elsewhere), clamped to [Min, Max] and, for an int, rounded. Text that
    /// is not a number is discarded and the value stays. The up and down arrow keys step the value; Escape
    /// discards the typing and leaves the field. Returns true on frames the value changed.
    ///
    /// The label identifies the field and is its placeholder while it is empty; put a Text next to it for a
    /// visible title.
    bool NumberField(std::string_view label, int* value, const NumberFieldOptions& options = {});
    bool NumberField(std::string_view label, float* value, const NumberFieldOptions& options = {});
    bool NumberField(std::string_view label, double* value, const NumberFieldOptions& options = {});
} // namespace Carbon
