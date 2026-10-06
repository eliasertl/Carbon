#include "Carbon/Extensions/NumberField.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>

#include "Carbon/Extensions/Internal/NumberEditing.h"

namespace Carbon
{
    namespace
    {
        // The most decimals FormatNumber writes; a double has no more significant digits than this.
        constexpr int MaxDecimals = 15;

        std::string_view TrimSpaces(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
                text.remove_suffix(1);
            return text;
        }

        // Copies as much of `text` as fits, without cutting a character in two.
        char* Append(char* out, char* end, std::string_view text)
        {
            size_t count = std::min(text.size(), static_cast<size_t>(end - out));
            if (count < text.size())
            {
                while (count > 0 && (static_cast<unsigned char>(text[count]) & 0xC0) == 0x80)
                    count--;
            }
            std::memcpy(out, text.data(), count);
            return out + count;
        }
    } // namespace

    std::string_view FormatNumber(double value, const NumberFormat& format, std::span<char> buffer)
    {
        char* out = buffer.data();
        char* const end = buffer.data() + buffer.size();
        out = Append(out, end, format.Prefix);

        // Fixed notation of the largest double needs over 300 digits.
        char digits[400];
        std::string_view number;
        if (std::isnan(value))
        {
            number = "NaN";
        }
        else if (std::isinf(value))
        {
            number = value < 0.0 ? "-\xE2\x88\x9E" : "\xE2\x88\x9E"; // U+221E INFINITY
        }
        else
        {
            const int decimals = format.Decimals < 0 ? 3 : std::min(format.Decimals, MaxDecimals);
            const std::to_chars_result result =
                std::to_chars(digits, digits + sizeof(digits), value, std::chars_format::fixed, decimals);
            number = std::string_view(digits, result.ec == std::errc() ? result.ptr - digits : 0);
            // Automatic decimals drop trailing zeros, and the point when nothing follows it.
            if (format.Decimals < 0 && number.find('.') != std::string_view::npos)
            {
                while (number.back() == '0')
                    number.remove_suffix(1);
                if (number.back() == '.')
                    number.remove_suffix(1);
            }
            // A negative number that rounds to zero is written as zero.
            if (number.size() > 1 && number.front() == '-' &&
                number.find_first_not_of("0.", 1) == std::string_view::npos)
                number.remove_prefix(1);
        }
        out = Append(out, end, number);
        out = Append(out, end, format.Suffix);
        return std::string_view(buffer.data(), static_cast<size_t>(out - buffer.data()));
    }

    bool ParseNumber(std::string_view text, const NumberFormat& format, double* value)
    {
        CB_VERIFY(value != nullptr, "ParseNumber needs a double to write to");
        if (value == nullptr)
            return false;

        text = TrimSpaces(text);
        const std::string_view prefix = TrimSpaces(format.Prefix);
        const std::string_view suffix = TrimSpaces(format.Suffix);
        if (!prefix.empty() && text.starts_with(prefix))
            text = TrimSpaces(text.substr(prefix.size()));
        if (!suffix.empty() && text.ends_with(suffix))
            text = TrimSpaces(text.substr(0, text.size() - suffix.size()));
        if (!text.empty() && text.front() == '+')
            text.remove_prefix(1);

        // std::from_chars wants a decimal point; a single comma is taken for one when there is no point.
        char local[128];
        if (text.empty() || text.size() >= sizeof(local))
            return false;
        std::memcpy(local, text.data(), text.size());
        const bool hasPoint = text.find('.') != std::string_view::npos;
        const size_t comma = text.find(',');
        if (!hasPoint && comma != std::string_view::npos && text.find(',', comma + 1) == std::string_view::npos)
            local[comma] = '.';

        double parsed = 0.0;
        const char* const last = local + text.size();
        const std::from_chars_result result = std::from_chars(local, last, parsed, std::chars_format::general);
        if (result.ec != std::errc() || result.ptr != last || !std::isfinite(parsed))
            return false;
        *value = parsed;
        return true;
    }

    namespace
    {
        Internal::NumberEditOptions ToEditOptions(const NumberFieldOptions& options)
        {
            Internal::NumberEditOptions edit;
            edit.Min = options.Min;
            edit.Max = options.Max;
            edit.Step = options.Step;
            edit.Format = options.Format;
            edit.Width = options.Width;
            edit.ControlSize = options.ControlSize;
            edit.Disabled = options.Disabled;
            return edit;
        }
    } // namespace

    bool NumberField(std::string_view label, int* value, const NumberFieldOptions& options)
    {
        return Internal::EditNumberValue(label, value, Internal::NumberKind::Int, ToEditOptions(options));
    }

    bool NumberField(std::string_view label, float* value, const NumberFieldOptions& options)
    {
        return Internal::EditNumberValue(label, value, Internal::NumberKind::Float, ToEditOptions(options));
    }

    bool NumberField(std::string_view label, double* value, const NumberFieldOptions& options)
    {
        return Internal::EditNumberValue(label, value, Internal::NumberKind::Double, ToEditOptions(options));
    }
} // namespace Carbon
