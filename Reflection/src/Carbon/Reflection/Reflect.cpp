#include "Carbon/Reflection/Reflect.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>

#include "Carbon/Carbon.h"
#include "Carbon/Extensions/ColorWell.h"
#include "Carbon/Extensions/DatePicker.h"
#include "Carbon/Extensions/PopUpButton.h"
#include "Carbon/Extensions/RadioGroup.h"
#include "Carbon/Extensions/SegmentedControl.h"
#include "Carbon/Extensions/Stepper.h"

namespace Carbon::Internal
{
    namespace
    {
        // The spacing of the Gallery's forms: 12 points between rows and between a label and its control.
        constexpr float RowSpacing = 12.0f;
        constexpr float LabelSpacing = 12.0f;
        // LabelAbove: a label sits this close above its control.
        constexpr float LabelAboveSpacing = 4.0f;
        // A nested struct: its title, then its fields, indented.
        constexpr float GroupSpacing = 8.0f;
        constexpr float GroupIndent = 16.0f;
        // Between a slider or stepper and the value shown next to it.
        constexpr float ValueSpacing = 8.0f;
        // The narrowest the value of a stepper without a range is shown, so small changes do not move the stepper.
        constexpr float MinimumValueWidth = 40.0f;

        // Writes a number with exactly two decimals. std::to_chars rather than std::format: MSVC's std::format
        // allocates for floating-point numbers with a precision, and Reflect must not allocate in a settled frame.
        std::string_view FormatTwoDecimals(double value, std::span<char> buffer)
        {
            const std::to_chars_result result =
                std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, 2);
            if (result.ec != std::errc())
                return {};
            return std::string_view(buffer.data(), static_cast<size_t>(result.ptr - buffer.data()));
        }

        float MeasureNumber(double value, bool isInteger, bool allDecimals)
        {
            std::array<char, 48> buffer = {};
            const std::string_view text = allDecimals && !isInteger ? FormatTwoDecimals(value, buffer)
                                                                    : FormatReflectedNumber(value, isInteger, buffer);
            return MeasureText(text, GetTextSpec(TextStyle::Body)).X;
        }

        // The width of the value next to a slider or stepper: enough for every value in the range, so the control
        // beside it does not move while the value changes.
        float GetValueWidth(double value, const ReflectNumberSpec& spec)
        {
            const float current = MeasureNumber(value, spec.IsInteger, false);
            if (!spec.HasRange)
                return std::max(current, MinimumValueWidth);
            return std::max({current, MeasureNumber(spec.Min, spec.IsInteger, true),
                             MeasureNumber(spec.Max, spec.IsInteger, true)});
        }

        // The value next to a control, in a column of fixed width: after a slider it starts at the column's leading
        // edge, before a stepper it ends at the trailing edge, so it always sits next to its control.
        void ShowValue(double value, const ReflectNumberSpec& spec, float width, TextAlignment alignment)
        {
            std::array<char, 48> buffer = {};
            Text(FormatReflectedNumber(value, spec.IsInteger, buffer),
                 {.Width = Size::Fixed(width), .Alignment = alignment});
        }
    } // namespace

    void BeginReflect(std::string_view label, const ReflectOptions& options)
    {
        PushID(label);
        PushDisabled(options.Disabled);
    }

    void EndReflect()
    {
        PopDisabled();
        PopID();
    }

    void BeginReflectGroup()
    {
        BeginVStack({.Spacing = 0.0f, .Padding = EdgeInsets(GroupIndent, 0.0f, 0.0f, 0.0f)});
    }

    void EndReflectGroup()
    {
        EndVStack();
    }

    void BeginReflectFields(const ReflectOptions& options)
    {
        if (options.Layout == ReflectLayout::LabelLeading)
            BeginGrid({.HorizontalSpacing = LabelSpacing, .VerticalSpacing = RowSpacing});
        else
            BeginVStack({.Spacing = RowSpacing});
    }

    void EndReflectFields(const ReflectOptions& options)
    {
        if (options.Layout == ReflectLayout::LabelLeading)
            EndGrid();
        else
            EndVStack();
    }

    void BeginReflectField(std::string_view name, std::string_view label, const ReflectFieldOptions& field,
                           ReflectRowKind kind, const ReflectOptions& options)
    {
        PushID(name);
        if (field.ReadOnly)
            PushDisabled(true);

        if (options.Layout == ReflectLayout::LabelLeading)
        {
            GridRowOptions row;
            if (kind == ReflectRowKind::Tall)
                row.Alignment = VerticalAlignment::Top;
            BeginGridRow(row);
            switch (kind)
            {
                case ReflectRowKind::Default:
                case ReflectRowKind::Tall:
                    Text(label, {.Secondary = true});
                    break;
                case ReflectRowKind::Checkbox:
                    // The checkbox carries the label; the label column stays empty.
                    Spacer({.Length = 0.0f});
                    break;
                case ReflectRowKind::Group:
                    SetNextGridCell({.ColumnSpan = 2});
                    BeginVStack({.Spacing = GroupSpacing});
                    Text(label, {.Emphasized = true});
                    break;
            }
        }
        else
        {
            BeginVStack({.Spacing = kind == ReflectRowKind::Group ? GroupSpacing : LabelAboveSpacing});
            if (kind == ReflectRowKind::Group)
                Text(label, {.Emphasized = true});
            else if (kind != ReflectRowKind::Checkbox)
                Text(label, {.Secondary = true});
        }
    }

    void EndReflectField(const ReflectFieldOptions& field, ReflectRowKind kind, const ReflectOptions& options)
    {
        if (options.Layout == ReflectLayout::LabelLeading)
        {
            if (kind == ReflectRowKind::Group)
                EndVStack();
            EndGridRow();
        }
        else
        {
            EndVStack();
        }
        if (field.ReadOnly)
            PopDisabled();

        // The tooltip belongs to the whole row, label included.
        if (!field.Tooltip.empty())
        {
            const Rect row = GetLastItemRect();
            Interaction interaction;
            interaction.Hovered = IsRectHovered(row);
            SetLastItem(GetID("##row"), row, interaction);
            Tooltip(field.Tooltip);
        }
        PopID();
    }

    bool ReflectBool(std::string_view, std::string_view label, bool* value, ReflectControl control,
                     const ReflectOptions& options)
    {
        if (control == ReflectControl::Checkbox)
            return Toggle(label, value, {.Kind = ToggleKind::Checkbox, .ControlSize = options.ControlSize});
        // The label column shows the label; the switch has none of its own.
        return Toggle("##switch", value, {.ControlSize = options.ControlSize});
    }

    bool ReflectNumber(std::string_view, double* value, const ReflectNumberSpec& spec, const ReflectOptions& options)
    {
        const bool isSlider =
            spec.Control == ReflectControl::Slider || (spec.Control == ReflectControl::Automatic && spec.HasRange);
        const float valueWidth = GetValueWidth(*value, spec);
        bool changed = false;
        BeginHStack({.Spacing = ValueSpacing, .Alignment = VerticalAlignment::Center});
        if (isSlider)
        {
            float slider = static_cast<float>(*value);
            const float step = spec.HasStep ? static_cast<float>(spec.Step) : (spec.IsInteger ? 1.0f : 0.0f);
            if (Slider("##slider", &slider, static_cast<float>(spec.Min), static_cast<float>(spec.Max),
                       {.Step = step, .ControlSize = options.ControlSize}))
            {
                *value = static_cast<double>(slider);
                changed = true;
            }
            ShowValue(*value, spec, valueWidth, TextAlignment::Leading);
        }
        else
        {
            // As on macOS: the value, then the stepper that changes it.
            ShowValue(*value, spec, valueWidth, TextAlignment::Trailing);
            changed =
                Stepper("##stepper", value,
                        {.Min = spec.Min, .Max = spec.Max, .Step = spec.Step, .ControlSize = options.ControlSize});
        }
        EndHStack();
        return changed;
    }

    ReflectControl ResolveEnumControl(ReflectControl fieldControl, const ReflectOptions& options)
    {
        if (fieldControl == ReflectControl::PopUpButton || fieldControl == ReflectControl::SegmentedControl ||
            fieldControl == ReflectControl::RadioGroup)
            return fieldControl;
        switch (options.EnumStyle)
        {
            case ReflectEnumStyle::SegmentedControl:
                return ReflectControl::SegmentedControl;
            case ReflectEnumStyle::RadioGroup:
                return ReflectControl::RadioGroup;
            case ReflectEnumStyle::PopUpButton:
                break;
        }
        return ReflectControl::PopUpButton;
    }

    bool ReflectChoice(std::string_view name, int* index, std::span<const std::string_view> items,
                       ReflectControl control, const ReflectOptions& options)
    {
        switch (control)
        {
            case ReflectControl::SegmentedControl:
                return SegmentedControl(name, index, items, {.ControlSize = options.ControlSize});
            case ReflectControl::RadioGroup:
                return RadioGroup(name, index, items, {.ControlSize = options.ControlSize});
            default:
                return PopUpButton(name, index, items, {.ControlSize = options.ControlSize});
        }
    }

    bool ReflectText(std::string_view, std::string_view label, std::string* value, const ReflectOptions& options)
    {
        return TextField("##text", value, {.Placeholder = label, .ControlSize = options.ControlSize});
    }

    bool ReflectColor(std::string_view, Color* value, const ReflectOptions& options)
    {
        return ColorWell("##color", value, {.ControlSize = options.ControlSize});
    }

    bool ReflectDate(std::string_view, DateTime* value, const ReflectOptions& options)
    {
        return DatePicker("##date", value, {.ControlSize = options.ControlSize});
    }

    std::string_view FormatReflectedNumber(double value, bool isInteger, std::span<char> buffer)
    {
        if (buffer.empty())
            return {};
        if (isInteger)
        {
            const std::format_to_n_result<char*> result =
                std::format_to_n(buffer.data(), static_cast<std::ptrdiff_t>(buffer.size()), "{}",
                                 static_cast<long long>(std::llround(value)));
            return std::string_view(buffer.data(), std::min(static_cast<size_t>(result.size), buffer.size()));
        }

        // At most two decimals, without trailing zeros: 2.2, 0.75, 1. Rounding to zero must not print "-0".
        double rounded = std::round(value * 100.0) / 100.0;
        if (rounded == 0.0)
            rounded = 0.0;
        std::string_view text = FormatTwoDecimals(rounded, buffer);
        if (text.find('.') != std::string_view::npos)
        {
            while (!text.empty() && text.back() == '0')
                text.remove_suffix(1);
            if (!text.empty() && text.back() == '.')
                text.remove_suffix(1);
        }
        return text;
    }
} // namespace Carbon::Internal
