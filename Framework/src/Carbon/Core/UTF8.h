#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Carbon
{
    /// The replacement character U+FFFD, produced for malformed UTF-8.
    inline constexpr char32_t ReplacementCharacter = 0xFFFD;

    /// One decoded code point and the number of bytes it occupied.
    struct UTF8Decoded
    {
        char32_t Codepoint = 0;
        uint32_t Length = 0;
    };

    /// Decodes the code point starting at `offset`. Malformed input (bad lead or continuation bytes, overlong
    /// forms, surrogates, values above U+10FFFF, truncation) yields ReplacementCharacter with Length 1, so decoding
    /// always makes progress. Returns Length 0 only when `offset` is at or past the end.
    UTF8Decoded DecodeUTF8(std::string_view text, size_t offset);

    /// Encodes a code point into `out` (at least 4 bytes) and returns the number of bytes written. Invalid code
    /// points are encoded as ReplacementCharacter.
    uint32_t EncodeUTF8(char32_t codepoint, char* out);

    /// Appends a code point to a UTF-8 string.
    void AppendUTF8(std::string& text, char32_t codepoint);

    /// Counts the code points in a UTF-8 string (malformed bytes count as one each).
    size_t CountCodepoints(std::string_view text);

    /// Returns the byte offset of the code point after the one at `offset`, clamped to the end of the text.
    size_t NextCodepointOffset(std::string_view text, size_t offset);

    /// Returns the byte offset of the code point before `offset`, clamped to 0.
    size_t PreviousCodepointOffset(std::string_view text, size_t offset);
} // namespace Carbon
