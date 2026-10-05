#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace Carbon
{
    /// The font files compiled into the Carbon library.
    enum class EmbeddedFont : uint8_t
    {
        /// Public Sans, variable weight (100-900), upright.
        PublicSansRoman,
        /// Public Sans, variable weight (100-900), italic.
        PublicSansItalic,
        /// Phosphor icons, regular weight.
        PhosphorRegular,
        /// Phosphor icons, bold weight.
        PhosphorBold,
        /// Phosphor icons, filled.
        PhosphorFill,
        /// JetBrains Mono, variable weight (100-800), upright. The monospaced font.
        JetBrainsMonoRoman,
        /// JetBrains Mono, variable weight (100-800), italic.
        JetBrainsMonoItalic,

        Count
    };

    /// Returns the bytes of an embedded font file (TrueType). The data lives for the whole program.
    std::span<const uint8_t> GetEmbeddedFont(EmbeddedFont font);

    /// Returns the WGSL source of Carbon's shader.
    std::string_view GetEmbeddedShader();
} // namespace Carbon
