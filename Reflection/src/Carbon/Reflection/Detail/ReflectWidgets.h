#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Extensions/DateTime.h"
#include "Carbon/Reflection/ReflectFieldOptions.h"
#include "Carbon/Reflection/ReflectOptions.h"

// The non-template half of Reflect, implemented in Reflect.cpp: one function per kind of control, and the rows and
// groups around them. Reflect<T> calls these for each field.

namespace Carbon::Internal
{
    /// How a field's row is laid out.
    enum class ReflectRowKind : uint8_t
    {
        /// Label, then control.
        Default,
        /// A control taller than one line (radio buttons): the label sits at the top.
        Tall,
        /// A checkbox, which carries its own label after the box.
        Checkbox,
        /// A nested struct: a title, then its fields, indented.
        Group
    };

    /// Pushes the ID of a Reflect call and opens its disabled scope.
    void BeginReflect(std::string_view label, const ReflectOptions& options);
    void EndReflect();

    /// Indents the fields of a nested struct, whose title BeginReflectField drew, until EndReflectGroup.
    void BeginReflectGroup();
    void EndReflectGroup();

    /// Starts the fields of one struct: a grid of labels and controls, or a column.
    void BeginReflectFields(const ReflectOptions& options);
    void EndReflectFields(const ReflectOptions& options);

    /// Starts the row of one field: pushes the field's ID, opens the row and draws the label. Read-only fields are
    /// disabled until EndReflectField, which also attaches the tooltip to the row.
    void BeginReflectField(std::string_view name, std::string_view label, const ReflectFieldOptions& field,
                           ReflectRowKind kind, const ReflectOptions& options);
    void EndReflectField(const ReflectFieldOptions& field, ReflectRowKind kind, const ReflectOptions& options);

    /// A switch or a checkbox. `label` is drawn only by a checkbox.
    bool ReflectBool(std::string_view name, std::string_view label, bool* value, ReflectControl control,
                     const ReflectOptions& options);

    /// How a number is edited.
    struct ReflectNumberSpec
    {
        ReflectControl Control = ReflectControl::Automatic;
        double Min = 0.0;
        double Max = 0.0;
        double Step = 1.0;
        /// Whether Step was given; a slider without one is continuous for floating-point numbers.
        bool HasStep = false;
        bool HasRange = false;
        bool IsInteger = false;
    };

    /// A slider (with a range) or a stepper with the value next to it. `value` holds the number as a double.
    bool ReflectNumber(std::string_view name, double* value, const ReflectNumberSpec& spec,
                       const ReflectOptions& options);

    /// A pop-up button, segmented control or radio group choosing among `items`; `control` is already resolved.
    bool ReflectChoice(std::string_view name, int* index, std::span<const std::string_view> items,
                       ReflectControl control, const ReflectOptions& options);

    /// The enum control a field gets: the field's override, else the call's EnumStyle.
    ReflectControl ResolveEnumControl(ReflectControl fieldControl, const ReflectOptions& options);

    bool ReflectText(std::string_view name, std::string_view label, std::string* value, const ReflectOptions& options);
    bool ReflectColor(std::string_view name, Color* value, const ReflectOptions& options);
    bool ReflectDate(std::string_view name, DateTime* value, const ReflectOptions& options);

    /// Writes a number as Reflect shows it: integers in full, others with at most two decimals and without
    /// trailing zeros. Returns a view of `buffer`.
    std::string_view FormatReflectedNumber(double value, bool isInteger, std::span<char> buffer);
} // namespace Carbon::Internal
