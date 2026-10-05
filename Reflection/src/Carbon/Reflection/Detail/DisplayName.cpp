#include "Carbon/Reflection/Detail/DisplayName.h"

namespace Carbon::Internal
{
    namespace
    {
        bool IsLower(char character)
        {
            return character >= 'a' && character <= 'z';
        }

        bool IsUpper(char character)
        {
            return character >= 'A' && character <= 'Z';
        }

        bool IsDigit(char character)
        {
            return character >= '0' && character <= '9';
        }

        char ToUpper(char character)
        {
            return IsLower(character) ? static_cast<char>(character - 'a' + 'A') : character;
        }

        // Whether a word starts at `index` (which is not the first character of a word already).
        bool StartsWord(std::string_view identifier, size_t index)
        {
            const char previous = identifier[index - 1];
            const char current = identifier[index];
            const char next = index + 1 < identifier.size() ? identifier[index + 1] : '\0';
            // "darkMode", "Volume2"
            if (IsLower(previous) && (IsUpper(current) || IsDigit(current)))
                return true;
            // The last capital of an acronym starts the next word: "HDREnabled", "VSync"; and after a number:
            // "MP3Player", "2Factor". A capital that ends the identifier stays: "UserID", "Vector3D".
            return (IsUpper(previous) || IsDigit(previous)) && IsUpper(current) && IsLower(next);
        }
    } // namespace

    size_t FormatDisplayName(std::string_view identifier, std::span<char> output) noexcept
    {
        size_t length = 0;
        bool isWordStart = true;
        for (size_t i = 0; i < identifier.size(); i++)
        {
            char character = identifier[i];
            if (character == '_')
            {
                isWordStart = true;
                continue;
            }
            if (!isWordStart && i > 0 && StartsWord(identifier, i))
                isWordStart = true;

            if (isWordStart)
            {
                if (length > 0 && length < output.size())
                    output[length++] = ' ';
                character = ToUpper(character);
                isWordStart = false;
            }
            if (length < output.size())
                output[length++] = character;
        }
        return length;
    }
} // namespace Carbon::Internal
