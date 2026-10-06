#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/State.h"

namespace Carbon::Internal
{
    /// Per-ID state blocks of one context. See Carbon::GetState.
    ///
    /// A block is found through an open-addressing table of (key, block) pairs. The blocks themselves live in
    /// chunks that never move, so a pointer handed out stays valid, and the memory of dropped transient state is
    /// reused for new state of the same size: a steady-state frame allocates nothing. Only transient blocks are
    /// looked at when a frame ends.
    class StateStorage
    {
    public:
        StateStorage();
        ~StateStorage();

        StateStorage(const StateStorage&) = delete;
        StateStorage& operator=(const StateStorage&) = delete;

        /// Returns the block for (id, typeKey), creating a zero-filled one of `size` bytes when needed.
        void* Get(ID id, const void* typeKey, size_t size, StateLifetime lifetime, uint64_t frameCount, bool* created);

        /// Drops transient blocks that were not requested during the frame that just ended.
        void EndFrame(uint64_t frameCount);

        size_t GetBlockCount() const { return m_BlockCount; }

    private:
        /// What precedes the bytes of a block.
        struct alignas(std::max_align_t) Block
        {
            uint64_t Key = 0;
            uint64_t LastUsedFrame = 0;
            uint32_t Size = 0;
            /// Position in m_Transient; NoIndex for persistent state.
            uint32_t TransientIndex = 0;
            /// The next free block of the same size class, while this one is unused.
            Block* NextFree = nullptr;
        };

        struct Slot
        {
            uint64_t Key = 0;
            /// Null for an empty slot.
            Block* Entry = nullptr;
        };

        static void* GetData(Block* block) { return block + 1; }

        size_t GetSlotIndex(uint64_t key) const;
        void Grow();
        Block* Allocate(size_t size);
        void Free(Block* block);
        void SetLifetime(Block* block, StateLifetime lifetime);
        void RemoveFromTransient(Block* block);
        void Erase(Block* block);

    private:
        std::vector<Slot> m_Slots;
        /// log2 of the table's size.
        uint32_t m_SlotBits = 0;
        size_t m_BlockCount = 0;
        /// The transient blocks: the only ones EndFrame visits.
        std::vector<Block*> m_Transient;

        /// Memory for the blocks up to MaxPooledSize, handed out front to back.
        std::vector<std::unique_ptr<std::byte[]>> m_Chunks;
        size_t m_ChunkUsed = 0;
        /// Unused blocks by size class (data size / SizeStep).
        std::vector<Block*> m_FreeBlocks;
    };
} // namespace Carbon::Internal
