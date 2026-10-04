#include "Carbon/Core/StateStorageInternal.h"

#include <cstring>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Hash.h"

namespace Carbon::Internal
{
    void* StateStorage::Get(ID id, const void* typeKey, size_t size, StateLifetime lifetime, uint64_t frameCount,
                            bool* created)
    {
        const uint64_t key = HashCombine(HashCombine(HashSeed, id.Value), reinterpret_cast<uintptr_t>(typeKey));
        Block& block = m_Blocks[key];
        const bool isNew = block.Data == nullptr;
        if (isNew)
        {
            block.Data = std::make_unique<std::byte[]>(size);
            block.Size = size;
            std::memset(block.Data.get(), 0, size);
        }
        CB_ASSERT(block.Size == size, "State block size changed for the same ID and type");
        block.LastUsedFrame = frameCount;
        block.Lifetime = lifetime;
        if (created != nullptr)
            *created = isNew;
        return block.Data.get();
    }

    void StateStorage::EndFrame(uint64_t frameCount)
    {
        for (auto it = m_Blocks.begin(); it != m_Blocks.end();)
        {
            const Block& block = it->second;
            if (block.Lifetime == StateLifetime::Transient && block.LastUsedFrame < frameCount)
                it = m_Blocks.erase(it);
            else
                ++it;
        }
    }

    void* GetStateBlock(ID id, const void* typeKey, size_t size, StateLifetime lifetime, bool* created)
    {
        Context& context = GetContext();
        return context.States.Get(id, typeKey, size, lifetime, context.FrameCount, created);
    }
} // namespace Carbon::Internal
