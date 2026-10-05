#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "Carbon/Reflection/ReflectEnumOptions.h"
#include "Carbon/Reflection/ReflectFieldOptions.h"

// What CB_REFLECT_STRUCT and CB_REFLECT_ENUM expand to. The macros define a function
// `CarbonReflectDescribe(TypeTag<T>)` next to the type; argument-dependent lookup finds it from here, so the type
// itself is never modified and the macro works for types in any namespace.

namespace Carbon::Internal
{
    /// Selects the description of `T` through argument-dependent lookup.
    template <typename T>
    struct TypeTag
    {
    };

    /// One field named by CB_FIELD.
    template <typename Owner, typename Member>
    struct FieldDescription
    {
        Member Owner::* Pointer = nullptr;
        std::string_view Name = {};
        ReflectFieldOptions Options = {};
    };

    /// The fields named by CB_REFLECT_STRUCT, in the order they were named.
    template <typename T, typename... FieldTypes>
    struct StructDescription
    {
        std::tuple<FieldTypes...> Fields;
    };

    /// One value named by CB_VALUE.
    template <typename E>
    struct ValueDescription
    {
        E Value = {};
        std::string_view Name = {};
        ReflectValueOptions Options = {};
    };

    /// The range and values named by CB_REFLECT_ENUM.
    template <typename E, size_t Count>
    struct EnumDescription
    {
        ReflectEnumOptions Options = {};
        std::array<ValueDescription<E>, Count> Values = {};
    };

    template <typename Owner, typename Member>
    constexpr FieldDescription<Owner, Member> DescribeField(Member Owner::* pointer, std::string_view name,
                                                            const ReflectFieldOptions& options) noexcept
    {
        static_assert(!std::is_member_function_pointer_v<Member Owner::*>, "CB_FIELD names a data member");
        return FieldDescription<Owner, Member>{pointer, name, options};
    }

    template <typename T, typename... FieldTypes>
    constexpr StructDescription<T, FieldTypes...> DescribeStruct(const FieldTypes&... fields) noexcept
    {
        return StructDescription<T, FieldTypes...>{std::tuple<FieldTypes...>(fields...)};
    }

    template <typename E>
    constexpr ValueDescription<E> DescribeValue(E value, std::string_view name,
                                                const ReflectValueOptions& options) noexcept
    {
        return ValueDescription<E>{value, name, options};
    }

    /// CB_REFLECT_ENUM(Type, CB_VALUE(...)...): the default range.
    template <typename E, typename... Values>
        requires(std::is_same_v<Values, ValueDescription<E>> && ...)
    constexpr EnumDescription<E, sizeof...(Values)> DescribeEnum(const Values&... values) noexcept
    {
        return EnumDescription<E, sizeof...(Values)>{ReflectEnumOptions{}, {values...}};
    }

    /// CB_REFLECT_ENUM(Type, { .Min = ..., .Max = ... }, CB_VALUE(...)...): a range of its own.
    template <typename E, typename... Values>
        requires(std::is_same_v<Values, ValueDescription<E>> && ...)
    constexpr EnumDescription<E, sizeof...(Values)> DescribeEnum(const ReflectEnumOptions& options,
                                                                 const Values&... values) noexcept
    {
        return EnumDescription<E, sizeof...(Values)>{options, {values...}};
    }

    /// True when a CB_REFLECT_STRUCT or CB_REFLECT_ENUM describes `T`.
    template <typename T>
    concept HasReflectDescription = requires { CarbonReflectDescribe(TypeTag<T>{}); };

    template <typename T>
        requires HasReflectDescription<T>
    constexpr auto GetReflectDescription() noexcept
    {
        return CarbonReflectDescribe(TypeTag<T>{});
    }
} // namespace Carbon::Internal
