#include "Carbon/Core/UTF8.h"

namespace Carbon
{
    namespace
    {
        bool IsContinuation(unsigned char byte)
        {
            return (byte & 0xC0) == 0x80;
        }
    } // namespace

    UTF8Decoded DecodeUTF8(std::string_view text, size_t offset)
    {
        if (offset >= text.size())
            return UTF8Decoded{0, 0};

        const unsigned char lead = static_cast<unsigned char>(text[offset]);
        if (lead < 0x80)
            return UTF8Decoded{lead, 1};

        uint32_t length = 0;
        char32_t codepoint = 0;
        char32_t minimum = 0;
        if ((lead & 0xE0) == 0xC0)
        {
            length = 2;
            codepoint = lead & 0x1F;
            minimum = 0x80;
        }
        else if ((lead & 0xF0) == 0xE0)
        {
            length = 3;
            codepoint = lead & 0x0F;
            minimum = 0x800;
        }
        else if ((lead & 0xF8) == 0xF0)
        {
            length = 4;
            codepoint = lead & 0x07;
            minimum = 0x10000;
        }
        else
        {
            return UTF8Decoded{ReplacementCharacter, 1};
        }

        if (offset + length > text.size())
            return UTF8Decoded{ReplacementCharacter, 1};

        for (uint32_t i = 1; i < length; i++)
        {
            const unsigned char byte = static_cast<unsigned char>(text[offset + i]);
            if (!IsContinuation(byte))
                return UTF8Decoded{ReplacementCharacter, 1};
            codepoint = (codepoint << 6) | (byte & 0x3F);
        }

        const bool isOverlong = codepoint < minimum;
        const bool isSurrogate = codepoint >= 0xD800 && codepoint <= 0xDFFF;
        if (isOverlong || isSurrogate || codepoint > 0x10FFFF)
            return UTF8Decoded{ReplacementCharacter, 1};

        return UTF8Decoded{codepoint, length};
    }

    uint32_t EncodeUTF8(char32_t codepoint, char* out)
    {
        const bool isSurrogate = codepoint >= 0xD800 && codepoint <= 0xDFFF;
        if (isSurrogate || codepoint > 0x10FFFF)
            codepoint = ReplacementCharacter;

        if (codepoint < 0x80)
        {
            out[0] = static_cast<char>(codepoint);
            return 1;
        }
        if (codepoint < 0x800)
        {
            out[0] = static_cast<char>(0xC0 | (codepoint >> 6));
            out[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
            return 2;
        }
        if (codepoint < 0x10000)
        {
            out[0] = static_cast<char>(0xE0 | (codepoint >> 12));
            out[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
            return 3;
        }
        out[0] = static_cast<char>(0xF0 | (codepoint >> 18));
        out[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
        return 4;
    }

    void AppendUTF8(std::string& text, char32_t codepoint)
    {
        char buffer[4];
        const uint32_t length = EncodeUTF8(codepoint, buffer);
        text.append(buffer, length);
    }

    size_t CountCodepoints(std::string_view text)
    {
        size_t count = 0;
        size_t offset = 0;
        while (offset < text.size())
        {
            offset += DecodeUTF8(text, offset).Length;
            count++;
        }
        return count;
    }

    size_t NextCodepointOffset(std::string_view text, size_t offset)
    {
        if (offset >= text.size())
            return text.size();
        return offset + DecodeUTF8(text, offset).Length;
    }

    size_t PreviousCodepointOffset(std::string_view text, size_t offset)
    {
        if (offset > text.size())
            offset = text.size();
        if (offset == 0)
            return 0;

        // Step back over at most three continuation bytes, then check that the sequence found really ends at
        // `offset`; otherwise the bytes are malformed and each one is its own (replacement) character.
        size_t start = offset - 1;
        size_t steps = 0;
        while (start > 0 && steps < 3 && IsContinuation(static_cast<unsigned char>(text[start])))
        {
            start--;
            steps++;
        }
        if (start + DecodeUTF8(text, start).Length == offset)
            return start;
        return offset - 1;
    }
} // namespace Carbon
