#pragma once

namespace Example
{
    /// Which of the system's fallback fonts were found.
    struct SystemFallbackFonts
    {
        /// Japanese, Chinese or Korean.
        bool HasCjk = false;
        /// Color emoji: Segoe UI Emoji, Noto Color Emoji or Apple Color Emoji.
        bool HasEmoji = false;
    };

    /// Adds fonts of the operating system to the current Carbon context as fallbacks for what the embedded fonts
    /// do not cover: Japanese, Chinese and Korean, so that text typed through an input method shows, and color
    /// emoji. Fonts that are not installed are skipped. Carbon embeds none of these; a real application ships or
    /// finds its own.
    SystemFallbackFonts AddSystemFallbackFonts();
} // namespace Example
