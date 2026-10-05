#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Carbon/Core/Color.h"
#include "Carbon/Extensions/DateTime.h"
#include "Carbon/Reflection/Detail/ReflectWidgets.h"
#include "Carbon/Reflection/Enum.h"
#include "Carbon/Reflection/ReflectFieldOptions.h"
#include "Carbon/Reflection/ReflectOptions.h"
#include "Carbon/Reflection/Struct.h"

namespace Carbon
{
    namespace Internal
    {
        template <typename>
        inline constexpr bool AlwaysFalse = false;

        /// A number of any arithmetic type, edited as a double and converted back with rounding and clamping.
        template <typename N>
        N ConvertReflectedNumber(double value) noexcept
        {
            if constexpr (std::is_integral_v<N>)
            {
                const double rounded = std::round(value);
                if (rounded <= static_cast<double>(std::numeric_limits<N>::lowest()))
                    return std::numeric_limits<N>::lowest();
                if (rounded >= static_cast<double>(std::numeric_limits<N>::max()))
                    return std::numeric_limits<N>::max();
                return static_cast<N>(rounded);
            }
            else
            {
                return static_cast<N>(value);
            }
        }

        template <typename N>
        bool ReflectArithmetic(std::string_view name, N* value, const ReflectFieldOptions& field,
                               const ReflectOptions& options)
        {
            ReflectNumberSpec spec;
            spec.Control = field.Control;
            spec.IsInteger = std::is_integral_v<N>;
            spec.HasRange = field.Min.has_value() && field.Max.has_value();
            spec.Min = field.Min.value_or(static_cast<double>(std::numeric_limits<N>::lowest()));
            spec.Max = field.Max.value_or(static_cast<double>(std::numeric_limits<N>::max()));
            spec.HasStep = field.Step.has_value();
            spec.Step = field.Step.value_or(std::is_integral_v<N> ? 1.0 : 0.1);

            double scratch = static_cast<double>(*value);
            if (!ReflectNumber(name, &scratch, spec, options))
                return false;
            const N next = ConvertReflectedNumber<N>(scratch);
            if (next == *value)
                return false;
            *value = next;
            return true;
        }

        template <typename E>
        bool ReflectEnumValue(std::string_view name, E* value, ReflectControl control, const ReflectOptions& options)
        {
            int index = GetEnumIndex(*value);
            if (!ReflectChoice(name, &index, GetEnumDisplayNames<E>(), control, options) || index < 0)
                return false;
            const E next = GetEnumValue<E>(static_cast<size_t>(index));
            if (next == *value)
                return false;
            *value = next;
            return true;
        }

        template <typename T>
        bool ReflectFields(T& value, const ReflectOptions& options);

        template <typename T, size_t Index>
        bool ReflectField(T& owner, const ReflectOptions& options)
        {
            using Model = StructModel<T>;
            // A reference: GCC 13 crashes copying the options into a constexpr local.
            constexpr const ReflectFieldOptions& field = Model::Options[Index];
            if constexpr (field.Hidden)
            {
                return false;
            }
            else
            {
                auto& member = Model::template Get<Index>(owner);
                using Field = std::remove_reference_t<decltype(member)>;
                static_assert(!std::is_const_v<Field>,
                              "Reflect cannot edit a const field; hide it with CB_FIELD(Name, { .Hidden = true })");
                constexpr ReflectControl control = field.Control;
                constexpr bool isAutomatic = control == ReflectControl::Automatic;
                constexpr bool isEnumControl = control == ReflectControl::PopUpButton ||
                                               control == ReflectControl::SegmentedControl ||
                                               control == ReflectControl::RadioGroup;

                const std::string_view name = Model::Names[Index];
                const std::string_view label = Model::GetLabels()[Index];
                ReflectRowKind kind = ReflectRowKind::Default;
                if constexpr (std::is_same_v<Field, bool>)
                {
                    if constexpr (control == ReflectControl::Checkbox)
                        kind = ReflectRowKind::Checkbox;
                }
                else if constexpr (std::is_enum_v<Field>)
                {
                    if (ResolveEnumControl(control, options) == ReflectControl::RadioGroup)
                        kind = ReflectRowKind::Tall;
                }
                else if constexpr (!std::is_arithmetic_v<Field> && !std::is_same_v<Field, std::string> &&
                                   !std::is_same_v<Field, Color> && !std::is_same_v<Field, DateTime> &&
                                   ReflectedStruct<Field>)
                {
                    kind = ReflectRowKind::Group;
                }

                bool changed = false;
                BeginReflectField(name, label, field, kind, options);
                if constexpr (std::is_same_v<Field, bool>)
                {
                    static_assert(
                        isAutomatic || control == ReflectControl::Switch || control == ReflectControl::Checkbox,
                        "A bool field takes ReflectControl::Switch or ReflectControl::Checkbox");
                    changed = ReflectBool(name, label, &member, control, options);
                }
                else if constexpr (std::is_arithmetic_v<Field>)
                {
                    static_assert(
                        isAutomatic || control == ReflectControl::Slider || control == ReflectControl::Stepper,
                        "A number field takes ReflectControl::Slider or ReflectControl::Stepper");
                    static_assert(control != ReflectControl::Slider || (field.Min.has_value() && field.Max.has_value()),
                                  "ReflectControl::Slider needs Min and Max");
                    changed = ReflectArithmetic(name, &member, field, options);
                }
                else if constexpr (std::is_enum_v<Field>)
                {
                    static_assert(isAutomatic || isEnumControl,
                                  "An enum field takes ReflectControl::PopUpButton, SegmentedControl or RadioGroup");
                    changed = ReflectEnumValue(name, &member, ResolveEnumControl(control, options), options);
                }
                else if constexpr (std::is_same_v<Field, std::string>)
                {
                    static_assert(isAutomatic, "A std::string field has no other control than its text field");
                    changed = ReflectText(name, label, &member, options);
                }
                else if constexpr (std::is_same_v<Field, Color>)
                {
                    static_assert(isAutomatic, "A Carbon::Color field has no other control than its color well");
                    changed = ReflectColor(name, &member, options);
                }
                else if constexpr (std::is_same_v<Field, DateTime>)
                {
                    static_assert(isAutomatic, "A Carbon::DateTime field has no other control than its date picker");
                    changed = ReflectDate(name, &member, options);
                }
                else if constexpr (ReflectedStruct<Field>)
                {
                    static_assert(isAutomatic, "A nested struct field has no control of its own");
                    BeginReflectGroup();
                    changed = ReflectFields(member, options);
                    EndReflectGroup();
                }
                else
                {
                    static_assert(AlwaysFalse<Field>,
                                  "Reflect shows fields of type bool, integers, float, double, std::string, "
                                  "Carbon::Color, Carbon::DateTime, enums and reflected structs; hide any other field "
                                  "with CB_FIELD(Name, { .Hidden = true })");
                }
                EndReflectField(field, kind, options);
                return changed;
            }
        }

        template <typename T>
        bool ReflectFields(T& value, const ReflectOptions& options)
        {
            bool changed = false;
            BeginReflectFields(options);
            // The comma fold draws every field, in order.
            [&]<size_t... Index>(std::index_sequence<Index...>)
            {
                ((changed = ReflectField<T, Index>(value, options) || changed), ...);
            }(std::make_index_sequence<StructModel<T>::Count>());
            EndReflectFields(options);
            return changed;
        }
    } // namespace Internal

    /// Draws a control for `value` that is generated from its type, and returns true on frames it changed.
    ///
    /// - An enum becomes a pop-up button, segmented control or radio group (ReflectOptions::EnumStyle) listing
    ///   its reflected values by their labels.
    /// - A reflected struct becomes one labeled control per field, by the field's type and CB_FIELD metadata; see
    ///   Docs/Reflection.md for the mapping. Nested structs become indented groups. True when any field changed.
    ///
    /// As with every Carbon control, the label identifies the control and is not drawn.
    template <typename T>
    bool Reflect(std::string_view label, T* value, const ReflectOptions& options = {})
    {
        static_assert(std::is_enum_v<T> || ReflectedStruct<T>,
                      "Reflect supports enums and reflected structs: aggregates (public fields, no constructors, no "
                      "base classes, at most 64 fields) and types described with CB_REFLECT_STRUCT");
        Internal::BeginReflect(label, options);
        bool changed = false;
        if constexpr (std::is_enum_v<T>)
        {
            changed = Internal::ReflectEnumValue(
                label, value, Internal::ResolveEnumControl(ReflectControl::Automatic, options), options);
        }
        else
        {
            changed = Internal::ReflectFields(*value, options);
        }
        Internal::EndReflect();
        return changed;
    }
} // namespace Carbon
