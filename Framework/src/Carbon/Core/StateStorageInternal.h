#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/State.h"

namespace Carbon::Internal
{
    /// Per-ID state blocks of one context. See Carbon::GetState.
    class StateStorage
    {
    public:
        /// Returns the block for (id, typeKey), creating a zero-filled one of `size` bytes when needed.
        void* Get(ID id, const void* typeKey, size_t size, StateLifetime lifetime, uint64_t frameCount, bool* created);

        /// Drops transient blocks that were not requested during the frame that just ended.
        void EndFrame(uint64_t frameCount);

        size_t GetBlockCount() const { return m_Blocks.size(); }

    private:
        struct Block
        {
            std::unique_ptr<std::byte[]> Data;
            size_t Size = 0;
            uint64_t LastUsedFrame = 0;
            StateLifetime Lifetime = StateLifetime::Transient;
        };

    private:
        std::unordered_map<uint64_t, Block> m_Blocks;
    };
} // namespace Carbon::Internal
