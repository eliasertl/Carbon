#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "Carbon/Reflection/Detail/Description.h"
#include "Carbon/Reflection/Detail/DisplayName.h"
#include "Carbon/Reflection/Detail/FieldTie.h"
#include "Carbon/Reflection/Detail/Signature.h"
#include "Carbon/Reflection/ReflectFieldOptions.h"

namespace Carbon::Internal
{
    /// Converts to anything. Brace-initializing an aggregate with N of these compiles exactly when it has at least
    /// N fields, which is how the field count is found. Only used in unevaluated contexts.
    struct AnyField
    {
        template <typename T>
        operator T() const noexcept;
    };

    template <size_t>
    using AnyFieldAt = AnyField;

    template <typename T, size_t... Index>
    constexpr bool IsInitializableWith(std::index_sequence<Index...>) noexcept
    {
        return requires { T{AnyFieldAt<Index>{}...}; };
    }

    /// The number of fields of an aggregate: the largest number of initializers it accepts.
    template <typename T, size_t Count = MaxReflectedFieldCount + 1>
    constexpr size_t CountFields() noexcept
    {
        if constexpr (Count == 0 || IsInitializableWith<T>(std::make_index_sequence<Count>()))
            return Count;
        else
            return CountFields<T, Count - 1>();
    }

    /// Whether `T` can be reflected without a macro: a class aggregate with at most MaxReflectedFieldCount fields.
    template <typename T>
    constexpr bool IsAutomaticStruct() noexcept
    {
        if constexpr (std::is_class_v<T> && std::is_aggregate_v<T> && !std::is_union_v<T>)
            return CountFields<T>() <= MaxReflectedFieldCount;
        else
            return false;
    }

    /// A pointer as a class-type template argument. Clang spells a bare pointer to a subobject without the
    /// subobject's name, but spells it in full inside a class.
    template <typename T>
    struct FieldPointer
    {
        const T* Pointer = nullptr;
    };

    template <typename T>
    struct FakeObject
    {
        const T Value;
    };

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif
    /// Never defined: only the addresses of its fields are used, while compiling. Types local to a function cannot
    /// be reflected, because this declaration needs linkage.
    template <typename T>
    extern const FakeObject<T> g_FakeObject;

    template <typename T, size_t Count, size_t Index>
    constexpr auto GetFieldPointer() noexcept
    {
        using Field =
            std::remove_cvref_t<std::tuple_element_t<Index, decltype(TieFields<Count>(g_FakeObject<T>.Value))>>;
        return FieldPointer<Field>{&std::get<Index>(TieFields<Count>(g_FakeObject<T>.Value))};
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    template <typename T, size_t Count, size_t... Index>
    constexpr std::array<std::string_view, Count> GetAutomaticFieldNames(std::index_sequence<Index...>) noexcept
    {
        return {GetNameOf<GetFieldPointer<T, Count, Index>()>()...};
    }

    /// The macro description of a struct, or an empty one.
    template <typename T>
    constexpr auto GetStructDescription() noexcept
    {
        if constexpr (HasReflectDescription<T>)
            return GetReflectDescription<T>();
        else
            return StructDescription<T>{};
    }

    template <typename T>
    constexpr size_t GetStructFieldCount() noexcept
    {
        if constexpr (IsAutomaticStruct<T>())
            return CountFields<T>();
        else
            return std::tuple_size_v<decltype(GetStructDescription<T>().Fields)>;
    }

    template <typename Description, size_t... Index>
    constexpr std::array<std::string_view, sizeof...(Index)> GetDescribedFieldNames(
        const Description& description, std::index_sequence<Index...>) noexcept
    {
        return {std::get<Index>(description.Fields).Name...};
    }

    template <typename Description, size_t... Index>
    constexpr std::array<ReflectFieldOptions, sizeof...(Index)> GetDescribedFieldOptions(
        const Description& description, std::index_sequence<Index...>) noexcept
    {
        return {std::get<Index>(description.Fields).Options...};
    }

    template <typename T>
    constexpr auto GetStructFieldNames() noexcept
    {
        constexpr size_t count = GetStructFieldCount<T>();
        if constexpr (IsAutomaticStruct<T>())
            return GetAutomaticFieldNames<T, count>(std::make_index_sequence<count>());
        else
            return GetDescribedFieldNames(GetStructDescription<T>(), std::make_index_sequence<count>());
    }

    /// The index of the field called `name`, or `count` when there is none.
    template <size_t Count>
    constexpr size_t FindFieldName(const std::array<std::string_view, Count>& names, std::string_view name) noexcept
    {
        for (size_t i = 0; i < Count; i++)
        {
            if (names[i] == name)
                return i;
        }
        return Count;
    }

    /// Whether every field the macro names was found automatically, so its metadata can be attached.
    template <typename T>
    constexpr bool AreDescribedFieldsFound() noexcept
    {
        constexpr auto description = GetStructDescription<T>();
        constexpr size_t described = std::tuple_size_v<decltype(description.Fields)>;
        constexpr auto names = GetStructFieldNames<T>();
        constexpr std::array<std::string_view, described> describedNames =
            GetDescribedFieldNames(description, std::make_index_sequence<described>());
        for (size_t i = 0; i < described; i++)
        {
            if (FindFieldName(names, describedNames[i]) == names.size())
                return false;
        }
        return true;
    }

    template <typename T>
    constexpr auto GetStructFieldOptions() noexcept
    {
        constexpr size_t count = GetStructFieldCount<T>();
        constexpr auto description = GetStructDescription<T>();
        constexpr size_t described = std::tuple_size_v<decltype(description.Fields)>;
        if constexpr (IsAutomaticStruct<T>())
        {
            // Automatic fields, with the metadata of the ones the macro names.
            static_assert(AreDescribedFieldsFound<T>(),
                          "CB_REFLECT_STRUCT names a field that automatic reflection does not see");
            constexpr auto names = GetStructFieldNames<T>();
            constexpr std::array<std::string_view, described> describedNames =
                GetDescribedFieldNames(description, std::make_index_sequence<described>());
            constexpr std::array<ReflectFieldOptions, described> describedOptions =
                GetDescribedFieldOptions(description, std::make_index_sequence<described>());
            std::array<ReflectFieldOptions, count> options = {};
            for (size_t i = 0; i < described; i++)
                options[FindFieldName(names, describedNames[i])] = describedOptions[i];
            return options;
        }
        else
        {
            return GetDescribedFieldOptions(description, std::make_index_sequence<count>());
        }
    }

    template <size_t Count>
    constexpr std::array<std::string_view, Count> GetDisplayNameOverrides(
        const std::array<ReflectFieldOptions, Count>& options) noexcept
    {
        std::array<std::string_view, Count> displayNames = {};
        for (size_t i = 0; i < Count; i++)
            displayNames[i] = options[i].DisplayName;
        return displayNames;
    }

    /// Everything reflection knows about a struct, computed while compiling.
    template <typename T>
    struct StructModel
    {
        static constexpr bool IsAutomatic = IsAutomaticStruct<T>();
        static constexpr bool IsReflected = IsAutomatic || HasReflectDescription<T>;
        static constexpr size_t Count = GetStructFieldCount<T>();
        static constexpr std::array<std::string_view, Count> Names = GetStructFieldNames<T>();
        static constexpr std::array<ReflectFieldOptions, Count> Options = GetStructFieldOptions<T>();
        static constexpr std::array<std::string_view, Count> DisplayNames = GetDisplayNameOverrides(Options);

        /// The field at `Index` of `value` (which may be const).
        template <size_t Index, typename Self>
        static constexpr auto& Get(Self& value) noexcept
        {
            static_assert(std::is_same_v<std::remove_const_t<Self>, T>);
            if constexpr (IsAutomatic)
                return std::get<Index>(TieFields<Count>(value));
            else
                return value.*(std::get<Index>(GetStructDescription<T>().Fields).Pointer);
        }

        /// The labels, formatted on first use and kept in static storage.
        static const std::array<std::string_view, Count>& GetLabels() noexcept
        {
            static const LabelTable<Count, GetLabelCapacity(Names, DisplayNames)> table(Names, DisplayNames);
            return table.GetLabels();
        }
    };
} // namespace Carbon::Internal
