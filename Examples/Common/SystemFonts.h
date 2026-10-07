#pragma once

namespace Example
{
    /// Adds fonts of the operating system to the current Carbon context as fallbacks for scripts the embedded fonts
    /// do not cover: Japanese, Chinese and Korean, so that text typed through an input method shows. Fonts that are
    /// not installed are skipped. Carbon embeds none of these; a real application ships or finds its own.
    void AddSystemFallbackFonts();
} // namespace Example
