#pragma once

#include "Carbon/Reflection/Detail/Description.h"
#include "Carbon/Reflection/ReflectEnumOptions.h"
#include "Carbon/Reflection/ReflectFieldOptions.h"

/// Describes a struct for reflection. Write it after the type, in the namespace that declares the type, followed
/// by a semicolon. The type itself is not modified.
///
///     CB_REFLECT_STRUCT(AudioSettings,
///         CB_FIELD(Volume, { .Min = 0.0, .Max = 1.0, .Tooltip = "Output level" }),
///         CB_FIELD(DeviceId, { .Hidden = true }));
///
/// For an aggregate (a plain struct), name only the fields that need metadata; the others are found
/// automatically. For any other type (constructors, base classes, ...), the listed fields are the only ones shown,
/// in the listed order; use CB_FIELD(Name, {}) for a field without metadata. A type name that contains commas must
/// be given through an alias.
#define CB_REFLECT_STRUCT(Type, ...)                                                 \
    constexpr auto CarbonReflectDescribe(::Carbon::Internal::TypeTag<Type>) noexcept \
    {                                                                                \
        using CarbonReflectedType = Type;                                            \
        return ::Carbon::Internal::DescribeStruct<CarbonReflectedType>(__VA_ARGS__); \
    }                                                                                \
    static_assert(true, "")

/// One field inside CB_REFLECT_STRUCT: its name and a braced ReflectFieldOptions, e.g. `{ .DisplayName = "Name" }`.
#define CB_FIELD(Name, ...) \
    ::Carbon::Internal::DescribeField(&CarbonReflectedType::Name, #Name, ::Carbon::ReflectFieldOptions __VA_ARGS__)

/// Describes an enum for reflection: optionally a braced ReflectEnumOptions with the range to search, then
/// CB_VALUE entries. Write it after the enum, in the namespace that declares it, followed by a semicolon.
///
///     CB_REFLECT_ENUM(Quality, { .Min = 0, .Max = 255 },
///         CB_VALUE(VeryHigh, { .DisplayName = "Ultra" }));
///
/// Values named with CB_VALUE are shown even when they lie outside the searched range.
#define CB_REFLECT_ENUM(Type, ...)                                                   \
    constexpr auto CarbonReflectDescribe(::Carbon::Internal::TypeTag<Type>) noexcept \
    {                                                                                \
        using CarbonReflectedType = Type;                                            \
        return ::Carbon::Internal::DescribeEnum<CarbonReflectedType>(__VA_ARGS__);   \
    }                                                                                \
    static_assert(true, "")

/// One value inside CB_REFLECT_ENUM: its enumerator and a braced ReflectValueOptions.
#define CB_VALUE(Value, ...) \
    ::Carbon::Internal::DescribeValue(CarbonReflectedType::Value, #Value, ::Carbon::ReflectValueOptions __VA_ARGS__)
