#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Carbon/Core/ID.h"

namespace Carbon
{
    /// How long per-ID state lives.
    enum class StateLifetime : uint8_t
    {
        /// Dropped as soon as a whole frame passes without the state being requested. Right for anything that
        /// should start fresh when a widget reappears: animations, measured sizes, hover timers.
        Transient,
        /// Kept until the context is destroyed. Right for what the user expects to survive while a widget is
        /// hidden: scroll offsets, split positions, edit state.
        Persistent
    };

    namespace Internal
    {
        /// Returns the storage block for (id, type), creating a zero-filled one when needed.
        void* GetStateBlock(ID id, const void* typeKey, size_t size, StateLifetime lifetime, bool* created);

        /// One address per type, used as the type's key.
        template <typename T>
        struct StateTypeKey
        {
            static inline const char Value = 0;
        };
    } // namespace Internal

    /// Returns the piece of state of type T that belongs to `id` in the current context. Immediate-mode widgets
    /// keep what must survive between frames here. The state is zero-initialized when it is created; `created`,
    /// if given, tells whether that happened in this call. T must be trivially copyable.
    ///
    /// The pointer stays valid until the end of the frame.
    template <typename T>
    T* GetState(ID id, StateLifetime lifetime = StateLifetime::Transient, bool* created = nullptr)
    {
        static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>,
                      "Per-ID state must be trivially copyable");
        static_assert(alignof(T) <= alignof(std::max_align_t), "Over-aligned state is not supported");
        return static_cast<T*>(
            Internal::GetStateBlock(id, &Internal::StateTypeKey<T>::Value, sizeof(T), lifetime, created));
    }
} // namespace Carbon
