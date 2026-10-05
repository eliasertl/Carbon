#pragma once

#include <cstddef>
#include <string_view>
#include <utility>

#include "Carbon/Reflection/Detail/StructModel.h"
#include "Carbon/Reflection/ReflectFieldOptions.h"

namespace Carbon
{
    /// A struct that reflection can see: an aggregate (public fields, no constructors, no base classes) with at
    /// most 64 fields, or any type that CB_REFLECT_STRUCT describes.
    template <typename T>
    concept ReflectedStruct = Internal::StructModel<T>::IsReflected;

    /// The most fields a struct can have to be reflected without CB_REFLECT_STRUCT.
    inline constexpr size_t MaxReflectedFieldCount = Internal::MaxReflectedFieldCount;

    /// The number of fields of a reflected struct, hidden ones included.
    template <ReflectedStruct T>
    constexpr size_t GetFieldCount() noexcept
    {
        return Internal::StructModel<T>::Count;
    }

    /// The identifier of the field at `index`, such as "TextureQuality".
    template <ReflectedStruct T>
    constexpr std::string_view GetFieldName(size_t index) noexcept
    {
        return Internal::StructModel<T>::Names[index];
    }

    /// The metadata of the field at `index`: what CB_FIELD gave, or the defaults.
    template <ReflectedStruct T>
    constexpr const ReflectFieldOptions& GetFieldOptions(size_t index) noexcept
    {
        return Internal::StructModel<T>::Options[index];
    }

    /// The label of the field at `index`: its DisplayName, or the identifier split into words ("Texture
    /// Quality"). The text is in static storage.
    template <ReflectedStruct T>
    std::string_view GetFieldDisplayName(size_t index) noexcept
    {
        return Internal::StructModel<T>::GetLabels()[index];
    }

    /// The field at `Index` of `value`.
    template <size_t Index, ReflectedStruct T>
    constexpr auto& GetField(T& value) noexcept
    {
        static_assert(Index < GetFieldCount<T>(), "Field index out of range");
        return Internal::StructModel<T>::template Get<Index>(value);
    }

    /// The field at `Index` of `value`.
    template <size_t Index, ReflectedStruct T>
    constexpr const auto& GetField(const T& value) noexcept
    {
        static_assert(Index < GetFieldCount<T>(), "Field index out of range");
        return Internal::StructModel<T>::template Get<Index>(value);
    }

    /// Calls `function(index, field)` for every field of `value`, in order; `field` is a reference to the field,
    /// const if `value` is. The index works with GetFieldName, GetFieldDisplayName and GetFieldOptions.
    template <typename T, typename Function>
        requires ReflectedStruct<std::remove_const_t<T>>
    constexpr void ForEachField(T& value, Function&& function)
    {
        using Model = Internal::StructModel<std::remove_const_t<T>>;
        [&]<size_t... Index>(std::index_sequence<Index...>)
        { (function(Index, Model::template Get<Index>(value)), ...); }(std::make_index_sequence<Model::Count>());
    }
} // namespace Carbon
