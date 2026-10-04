#pragma once

#include <string_view>

namespace Carbon
{
    /// Seconds the pointer has to rest on an item before its tooltip appears.
    inline constexpr float TooltipDelay = 0.7f;

    /// Attaches a tooltip to the item submitted just before: a short description that appears next to the
    /// pointer after it has rested on the item for a moment, and disappears when it moves away or a button is
    /// pressed. The HIG recommends a sentence fragment that says what the control does, without ending
    /// punctuation.
    ///
    ///     Carbon::Button("Restore");
    ///     Carbon::Tooltip("Restore default settings");
    void Tooltip(std::string_view text);
} // namespace Carbon
