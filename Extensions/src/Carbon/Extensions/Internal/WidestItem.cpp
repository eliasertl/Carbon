#include "Carbon/Extensions/Internal/WidestItem.h"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace Carbon::Internal
{
    namespace
    {
        struct WidestItemState
        {
            /// Hash of what the width was measured for; 0 while there is none.
            uint64_t Key;
            float Width;
        };
    } // namespace

    float MeasureWidestItem(ID id, std::span<const std::string_view> items, const TextSpec& spec)
    {
        uint64_t key = HashCombine(HashSeed, reinterpret_cast<uintptr_t>(spec.Font));
        key = HashCombine(key, (static_cast<uint64_t>(std::bit_cast<uint32_t>(spec.Size)) << 32) |
                                   std::bit_cast<uint32_t>(spec.Tracking));
        key = HashCombine(key, (static_cast<uint64_t>(spec.Weight) << 16) | (static_cast<uint64_t>(spec.Icons) << 8) |
                                   (spec.Italic ? 1u : 0u));
        for (const std::string_view item : items)
            key = HashBytes(item, key);
        key = key == 0 ? 1 : key;

        WidestItemState& state = *GetState<WidestItemState>(HashID("##widest", id));
        if (state.Key != key)
        {
            float widest = 0.0f;
            for (const std::string_view item : items)
                widest = std::max(widest, MeasureText(item, spec).X);
            state.Key = key;
            state.Width = widest;
        }
        return state.Width;
    }
} // namespace Carbon::Internal
