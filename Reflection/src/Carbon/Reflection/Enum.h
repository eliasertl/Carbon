#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

#include "Carbon/Reflection/Detail/EnumModel.h"

namespace Carbon
{
    /// Any enum. Its values are found automatically in the range [-128, 127], or in the range and among the values
    /// that CB_REFLECT_ENUM gives. An enum without values in the range reflects as empty.
    template <typename E>
    concept ReflectedEnum = std::is_enum_v<E>;

    /// The number of reflected values of an enum (hidden values do not count).
    template <ReflectedEnum E>
    constexpr size_t GetEnumCount() noexcept
    {
        return Internal::EnumModel<E>::Count;
    }

    /// The reflected value at `index`, in ascending order of value. `index` must be below GetEnumCount.
    template <ReflectedEnum E>
    constexpr E GetEnumValue(size_t index) noexcept
    {
        return Internal::EnumModel<E>::Values[index];
    }

    /// All reflected values, in ascending order.
    template <ReflectedEnum E>
    constexpr std::span<const E> GetEnumValues() noexcept
    {
        return Internal::EnumModel<E>::Values;
    }

    /// The index of `value` among the reflected values, or -1 when it is not one of them (outside the searched
    /// range, hidden, or no enumerator).
    template <ReflectedEnum E>
    constexpr int GetEnumIndex(E value) noexcept
    {
        return Internal::EnumModel<E>::IndexOf(value);
    }

    /// The enumerator's identifier, such as "VeryHigh"; empty when the value is not reflected.
    template <ReflectedEnum E>
    constexpr std::string_view GetEnumName(E value) noexcept
    {
        const int index = GetEnumIndex(value);
        return index < 0 ? std::string_view() : Internal::EnumModel<E>::Names[static_cast<size_t>(index)];
    }

    /// The label of a value: the macro's DisplayName, or the identifier split into words ("Very High"). Empty when
    /// the value is not reflected. The text is in static storage.
    template <ReflectedEnum E>
    std::string_view GetEnumDisplayName(E value) noexcept
    {
        const int index = GetEnumIndex(value);
        return index < 0 ? std::string_view() : Internal::EnumModel<E>::GetLabels()[static_cast<size_t>(index)];
    }

    /// The labels of all reflected values, in the order of GetEnumValue: ready to pass to a PopUpButton,
    /// SegmentedControl or RadioGroup.
    template <ReflectedEnum E>
    std::span<const std::string_view> GetEnumDisplayNames() noexcept
    {
        return Internal::EnumModel<E>::GetLabels();
    }
} // namespace Carbon
