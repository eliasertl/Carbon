#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace Carbon::Internal
{
    /// Turns an identifier into a label: words are split at underscores and at changes of case, and the first
    /// letter of every word is capitalized. "DarkMode", "darkMode" and "dark_mode" become "Dark Mode"; runs of
    /// capitals stay together as an acronym ("HDREnabled" becomes "HDR Enabled"); digits start a word after a
    /// lowercase letter and stay with capitals ("Volume2" becomes "Volume 2", "MP3Player" "MP3 Player").
    /// Writes at most twice the identifier's length into `output` and returns the label's length.
    size_t FormatDisplayName(std::string_view identifier, std::span<char> output) noexcept;

    /// The labels of a reflected type, formatted once and kept in static storage. Each label is the given display
    /// name, or one formatted from the identifier when that is empty.
    template <size_t Count, size_t Capacity>
    class LabelTable
    {
    public:
        LabelTable(const std::array<std::string_view, Count>& identifiers,
                   const std::array<std::string_view, Count>& displayNames) noexcept
        {
            size_t used = 0;
            for (size_t i = 0; i < Count; i++)
            {
                if (!displayNames[i].empty())
                {
                    m_Labels[i] = displayNames[i];
                    continue;
                }
                const size_t length =
                    FormatDisplayName(identifiers[i], std::span<char>(m_Text.data() + used, identifiers[i].size() * 2));
                m_Labels[i] = std::string_view(m_Text.data() + used, length);
                used += identifiers[i].size() * 2;
            }
        }

        // The labels point into the table's own storage.
        LabelTable(const LabelTable&) = delete;
        LabelTable& operator=(const LabelTable&) = delete;

        const std::array<std::string_view, Count>& GetLabels() const noexcept { return m_Labels; }

    private:
        std::array<char, Capacity> m_Text = {};
        std::array<std::string_view, Count> m_Labels = {};
    };

    /// The storage a LabelTable needs for these identifiers and display names.
    template <size_t Count>
    constexpr size_t GetLabelCapacity(const std::array<std::string_view, Count>& identifiers,
                                      const std::array<std::string_view, Count>& displayNames) noexcept
    {
        size_t capacity = 0;
        for (size_t i = 0; i < Count; i++)
        {
            if (displayNames[i].empty())
                capacity += identifiers[i].size() * 2;
        }
        return capacity;
    }
} // namespace Carbon::Internal
