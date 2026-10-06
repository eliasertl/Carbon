#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace Carbon
{
    /// Start value of a hash.
    inline constexpr uint64_t HashSeed = 14695981039346656037ull;

    /// Mixes an integer into a hash: two multiplications, of which one is on the path from `seed` to the result.
    /// Every bit of both inputs reaches every bit of the result, and the order of the inputs matters.
    constexpr uint64_t HashCombine(uint64_t seed, uint64_t value)
    {
        uint64_t hash = seed + 0x9E3779B97F4A7C15ull + value * 0xBF58476D1CE4E5B9ull;
        hash ^= hash >> 32;
        hash *= 0x94D049BB133111EBull;
        hash ^= hash >> 29;
        return hash;
    }

    namespace Internal
    {
        /// Up to eight bytes as one little-endian integer.
        constexpr uint64_t ReadHashWord(const char* bytes, size_t count)
        {
            if (!std::is_constant_evaluated() && count == 8)
            {
                // One load. The byte loop below is what a compile-time evaluation can do.
                uint64_t word = 0;
                std::memcpy(&word, bytes, 8);
                return word;
            }
            uint64_t word = 0;
            for (size_t i = 0; i < count; i++)
                word |= static_cast<uint64_t>(static_cast<unsigned char>(bytes[i])) << (i * 8);
            return word;
        }
    } // namespace Internal

    /// Hashes a byte range, continuing from `seed`, eight bytes at a time. The length takes part, so a range and
    /// the same range with a zero byte appended hash differently.
    constexpr uint64_t HashBytes(std::string_view bytes, uint64_t seed = HashSeed)
    {
        const char* data = bytes.data();
        size_t remaining = bytes.size();
        uint64_t hash = seed;
        while (remaining >= 8)
        {
            hash = HashCombine(hash, Internal::ReadHashWord(data, 8));
            data += 8;
            remaining -= 8;
        }
        // The last, partial word has seven bytes at most; the length goes into the byte that is left.
        const uint64_t tail = Internal::ReadHashWord(data, remaining) | (static_cast<uint64_t>(bytes.size()) << 56);
        return HashCombine(hash, tail);
    }
} // namespace Carbon
