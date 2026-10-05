#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// The compiler tricks behind automatic reflection. Everything that depends on how a compiler spells a template
// argument in a function signature lives in this file, so that a backend built on standard reflection (C++26
// std::meta) can replace it without touching anything else.

namespace Carbon::Internal
{
    /// The compiler's name of this function. It spells out `Value`: an enumerator (`Quality::High`), a number for a
    /// value that is no enumerator (`(Quality)5`), or the path of the subobject a pointer points to. Not noexcept:
    /// MSVC would append that to the signature.
    template <auto Value>
    constexpr auto GetSignature()
    {
#if defined(__clang__) || defined(__GNUC__)
        return std::string_view(__PRETTY_FUNCTION__);
#elif defined(_MSC_VER)
        return std::string_view(__FUNCSIG__);
#else
#error "Carbon's reflection needs __PRETTY_FUNCTION__ or __FUNCSIG__"
#endif
    }

    constexpr bool IsIdentifierCharacter(char character) noexcept
    {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '_';
    }

    /// The identifier a signature from GetSignature ends with: "High" for `Quality::High`, "Gamma" for a pointer to
    /// `settings.Gamma`. Empty when the signature ends in a number, which is how compilers spell an enum value that
    /// has no enumerator.
    ///
    /// The signatures end like this:
    ///   GCC    `... [with auto Value = Quality::High]`  `... [with auto Value = FieldPointer<float>{(&
    ///   g.Value.S::Gamma)}]` Clang  `... [Value = Quality::High]`            `... [Value =
    ///   FieldPointer<float>{&g.Value.Gamma}]` MSVC   `...GetSignature<Quality::High>(void)`   `...GetSignature<struct
    ///   FieldPointer<float>{const float*:&g->Value->Gamma}>(void)`
    constexpr std::string_view GetTrailingIdentifier(std::string_view signature) noexcept
    {
#if defined(__clang__) || defined(__GNUC__)
        constexpr std::string_view suffix = "]";
#else
        constexpr std::string_view suffix = ">(void)";
#endif
        if (signature.size() < suffix.size() || signature.substr(signature.size() - suffix.size()) != suffix)
            return {};
        signature.remove_suffix(suffix.size());
        while (!signature.empty() && (signature.back() == ')' || signature.back() == '}' || signature.back() == ' '))
            signature.remove_suffix(1);

        size_t start = signature.size();
        while (start > 0 && IsIdentifierCharacter(signature[start - 1]))
            start--;
        const std::string_view identifier = signature.substr(start);
        if (identifier.empty() || (identifier.front() >= '0' && identifier.front() <= '9'))
            return {};
        return identifier;
    }

    /// A string of known length stored in a constexpr variable, so that only the identifier, not the whole
    /// signature, ends up in the program.
    template <size_t Length>
    struct FixedName
    {
        std::array<char, Length> Characters = {};

        constexpr std::string_view GetView() const noexcept { return std::string_view(Characters.data(), Length); }
    };

    template <auto Value>
    constexpr auto MakeFixedName() noexcept
    {
        constexpr std::string_view identifier = GetTrailingIdentifier(GetSignature<Value>());
        FixedName<identifier.size()> name;
        for (size_t i = 0; i < identifier.size(); i++)
            name.Characters[i] = identifier[i];
        return name;
    }

    template <auto Value>
    inline constexpr auto g_NameOf = MakeFixedName<Value>();

    /// The identifier that names `Value` (see GetTrailingIdentifier), in static storage.
    template <auto Value>
    constexpr std::string_view GetNameOf() noexcept
    {
        return g_NameOf<Value>.GetView();
    }
} // namespace Carbon::Internal
