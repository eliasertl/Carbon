#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace Carbon
{
    /// Identifies a widget or a piece of per-widget state. Value 0 means "no ID".
    struct ID
    {
        uint64_t Value = 0;

        constexpr bool IsValid() const { return Value != 0; }
        constexpr bool operator==(const ID& other) const = default;
    };

    /// Hashes a label into an ID, continuing from `seed` (the parent scope).
    /// A label containing "###" is hashed from the "###" onwards only, so the text before it can change freely.
    ID HashID(std::string_view label, ID seed = {});

    /// Hashes an integer into an ID, continuing from `seed`. Useful for items in a loop.
    ID HashID(int64_t value, ID seed = {});

    /// Returns the part of a label that is displayed: everything before the first "##".
    std::string_view GetDisplayLabel(std::string_view label);

    /// Pushes a scope onto the ID stack of the current context; IDs created inside are hashed with it.
    void PushID(std::string_view id);
    /// Pushes an integer scope, e.g. a loop index.
    void PushID(int64_t id);
    /// Pushes an already computed ID as the scope.
    void PushID(ID id);
    /// Pops the scope pushed last.
    void PopID();

    /// Returns the ID a label gets inside the current ID scope.
    ID GetID(std::string_view label);
    /// Returns the ID an integer gets inside the current ID scope.
    ID GetID(int64_t value);
} // namespace Carbon

/// Lets ID be used as a key in unordered containers.
template <>
struct std::hash<Carbon::ID>
{
    size_t operator()(const Carbon::ID& id) const noexcept { return std::hash<uint64_t>()(id.Value); }
};
