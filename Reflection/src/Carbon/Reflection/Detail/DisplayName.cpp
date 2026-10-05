#include "Carbon/Reflection/Detail/DisplayName.h"

#include <array>

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

        char ToLower(char character)
        {
            return IsUpper(character) ? static_cast<char>(character - 'A' + 'a') : character;
        }

        // Words that title-style capitalization keeps in lower case inside a label: articles, coordinating
        // conjunctions and short prepositions (Apple's style guide).
        constexpr std::array<std::string_view, 25> MinorWords = {
            "a",   "an", "and",  "as", "at",   "but", "by", "for", "from", "in",  "into", "nor", "of",
            "off", "on", "onto", "or", "over", "per", "so", "the", "to",   "via", "with", "yet"};

        bool IsMinorWord(std::span<const char> word)
        {
            for (const std::string_view minor : MinorWords)
            {
                if (minor.size() != word.size())
                    continue;
                bool matches = true;
                for (size_t i = 0; i < word.size() && matches; i++)
                    matches = ToLower(word[i]) == minor[i];
                if (matches)
                    return true;
            }
            return false;
        }

        // Lowers the first letter of minor words that are neither the first nor the last word, unless the word is
        // written in capitals ("Launch At Login" becomes "Launch at Login", "HDR In OUT" stays).
        void LowerMinorWords(std::span<char> label)
        {
            size_t start = 0;
            bool isFirst = true;
            while (start < label.size())
            {
                size_t end = start;
                while (end < label.size() && label[end] != ' ')
                    end++;
                const bool isLast = end == label.size();
                const std::span<char> word = label.subspan(start, end - start);
                bool isCapitalized = !word.empty() && IsUpper(word[0]);
                for (size_t i = 1; i < word.size() && isCapitalized; i++)
                    isCapitalized = !IsUpper(word[i]);
                if (!isFirst && !isLast && isCapitalized && IsMinorWord(word))
                    word[0] = ToLower(word[0]);
                isFirst = false;
                start = end + 1;
            }
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
        LowerMinorWords(output.first(length));
        return length;
    }
} // namespace Carbon::Internal
