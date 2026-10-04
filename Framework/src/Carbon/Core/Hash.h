#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Carbon
{
    /// Start value of the 64-bit FNV-1a hash.
    inline constexpr uint64_t HashSeed = 14695981039346656037ull;

    /// 64-bit FNV-1a over a byte range, continuing from `seed`.
    constexpr uint64_t HashBytes(std::string_view bytes, uint64_t seed = HashSeed)
    {
        uint64_t hash = seed;
        for (const char byte : bytes)
        {
            hash ^= static_cast<unsigned char>(byte);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    /// Mixes an integer into a hash.
    constexpr uint64_t HashCombine(uint64_t seed, uint64_t value)
    {
        uint64_t hash = seed;
        for (int i = 0; i < 8; i++)
        {
            hash ^= (value >> (i * 8)) & 0xFF;
            hash *= 1099511628211ull;
        }
        return hash;
    }
} // namespace Carbon
