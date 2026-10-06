#include "Carbon/Core/StateStorageInternal.h"

#include <cstring>
#include <new>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Hash.h"

namespace Carbon::Internal
{
    namespace
    {
        constexpr uint32_t NoIndex = 0xFFFFFFFFu;
        constexpr uint32_t InitialSlotBits = 8;
        // Blocks are sized in steps of the strictest alignment. Larger ones than MaxPooledSize are rare (a
        // component's scratch state) and get an allocation of their own.
        constexpr size_t SizeStep = alignof(std::max_align_t);
        constexpr size_t MaxPooledSize = 1024;
        constexpr size_t ChunkSize = 64 * 1024;

        size_t RoundUp(size_t size)
        {
            return (size + SizeStep - 1) / SizeStep * SizeStep;
        }
    } // namespace

    StateStorage::StateStorage() : m_Slots(size_t(1) << InitialSlotBits), m_SlotBits(InitialSlotBits)
    {
        m_FreeBlocks.resize(MaxPooledSize / SizeStep + 1, nullptr);
    }

    StateStorage::~StateStorage()
    {
        for (const Slot& slot : m_Slots)
        {
            if (slot.Entry != nullptr && slot.Entry->Size > MaxPooledSize)
                ::operator delete(slot.Entry);
        }
    }

    void* StateStorage::Get(ID id, const void* typeKey, size_t size, StateLifetime lifetime, uint64_t frameCount,
                            bool* created)
    {
        const uint64_t key = HashCombine(HashCombine(HashSeed, id.Value), reinterpret_cast<uintptr_t>(typeKey));
        const size_t mask = m_Slots.size() - 1;
        size_t index = GetSlotIndex(key);
        while (m_Slots[index].Entry != nullptr)
        {
            if (m_Slots[index].Key == key)
            {
                Block* block = m_Slots[index].Entry;
                CB_ASSERT(block->Size == size, "State block size changed for the same ID and type");
                block->LastUsedFrame = frameCount;
                if ((block->TransientIndex != NoIndex) != (lifetime == StateLifetime::Transient))
                    SetLifetime(block, lifetime);
                if (created != nullptr)
                    *created = false;
                return GetData(block);
            }
            index = (index + 1) & mask;
        }

        // The table stays at most half full, so that a probe is short.
        if ((m_BlockCount + 1) * 2 > m_Slots.size())
        {
            Grow();
            const size_t grownMask = m_Slots.size() - 1;
            index = GetSlotIndex(key);
            while (m_Slots[index].Entry != nullptr)
                index = (index + 1) & grownMask;
        }

        Block* block = Allocate(size);
        block->Key = key;
        block->LastUsedFrame = frameCount;
        block->Size = static_cast<uint32_t>(size);
        block->TransientIndex = NoIndex;
        std::memset(GetData(block), 0, size);
        SetLifetime(block, lifetime);
        m_Slots[index] = Slot{key, block};
        m_BlockCount++;
        if (created != nullptr)
            *created = true;
        return GetData(block);
    }

    void StateStorage::EndFrame(uint64_t frameCount)
    {
        for (size_t i = 0; i < m_Transient.size();)
        {
            Block* block = m_Transient[i];
            if (block->LastUsedFrame < frameCount)
            {
                // Another block takes this position in the list and is looked at next.
                Erase(block);
                RemoveFromTransient(block);
                Free(block);
            }
            else
            {
                i++;
            }
        }
    }

    size_t StateStorage::GetSlotIndex(uint64_t key) const
    {
        // Fibonacci hashing: the high bits of the product depend on every bit of the key.
        return static_cast<size_t>((key * 0x9E3779B97F4A7C15ull) >> (64 - m_SlotBits));
    }

    void StateStorage::Grow()
    {
        std::vector<Slot> old(m_Slots.size() * 2);
        old.swap(m_Slots);
        m_SlotBits++;
        const size_t mask = m_Slots.size() - 1;
        for (const Slot& slot : old)
        {
            if (slot.Entry == nullptr)
                continue;
            size_t index = GetSlotIndex(slot.Key);
            while (m_Slots[index].Entry != nullptr)
                index = (index + 1) & mask;
            m_Slots[index] = slot;
        }
    }

    StateStorage::Block* StateStorage::Allocate(size_t size)
    {
        const size_t dataSize = RoundUp(size);
        if (dataSize > MaxPooledSize)
            return new (::operator new(sizeof(Block) + dataSize)) Block();

        Block*& freeBlock = m_FreeBlocks[dataSize / SizeStep];
        if (freeBlock != nullptr)
        {
            Block* block = freeBlock;
            freeBlock = block->NextFree;
            block->NextFree = nullptr;
            return block;
        }

        const size_t blockSize = sizeof(Block) + dataSize;
        if (m_Chunks.empty() || m_ChunkUsed + blockSize > ChunkSize)
        {
            m_Chunks.push_back(std::make_unique_for_overwrite<std::byte[]>(ChunkSize));
            m_ChunkUsed = 0;
        }
        Block* block = new (m_Chunks.back().get() + m_ChunkUsed) Block();
        m_ChunkUsed += blockSize;
        return block;
    }

    void StateStorage::Free(Block* block)
    {
        const size_t dataSize = RoundUp(block->Size);
        if (dataSize > MaxPooledSize)
        {
            ::operator delete(block);
            return;
        }
        Block*& freeBlock = m_FreeBlocks[dataSize / SizeStep];
        block->NextFree = freeBlock;
        freeBlock = block;
    }

    void StateStorage::SetLifetime(Block* block, StateLifetime lifetime)
    {
        if (lifetime == StateLifetime::Transient)
        {
            block->TransientIndex = static_cast<uint32_t>(m_Transient.size());
            m_Transient.push_back(block);
        }
        else if (block->TransientIndex != NoIndex)
        {
            RemoveFromTransient(block);
        }
    }

    void StateStorage::RemoveFromTransient(Block* block)
    {
        // The last block of the list fills the gap.
        Block* last = m_Transient.back();
        m_Transient[block->TransientIndex] = last;
        last->TransientIndex = block->TransientIndex;
        m_Transient.pop_back();
        block->TransientIndex = NoIndex;
    }

    void StateStorage::Erase(Block* block)
    {
        const size_t mask = m_Slots.size() - 1;
        size_t hole = GetSlotIndex(block->Key);
        while (m_Slots[hole].Entry != block)
            hole = (hole + 1) & mask;

        // Entries after the hole move up while that keeps them reachable from their own first slot, so that no
        // probe ends early at the hole.
        for (size_t next = (hole + 1) & mask; m_Slots[next].Entry != nullptr; next = (next + 1) & mask)
        {
            const size_t first = GetSlotIndex(m_Slots[next].Key);
            if (((next - first) & mask) >= ((next - hole) & mask))
            {
                m_Slots[hole] = m_Slots[next];
                hole = next;
            }
        }
        m_Slots[hole] = Slot();
        m_BlockCount--;
    }

    void* GetStateBlock(ID id, const void* typeKey, size_t size, StateLifetime lifetime, bool* created)
    {
        Context& context = GetContext();
        return context.States.Get(id, typeKey, size, lifetime, context.FrameCount, created);
    }
} // namespace Carbon::Internal
