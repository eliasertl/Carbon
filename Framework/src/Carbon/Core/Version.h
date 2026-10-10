#pragma once

#include <cstdint>
#include <string_view>

namespace Carbon
{
    /// Semantic version of the Carbon library.
    struct Version
    {
        uint32_t Major = 0;
        uint32_t Minor = 0;
        uint32_t Patch = 0;
    };

    /// Returns the version of the Carbon library this binary was built from.
    Version GetVersion();

    /// Returns the version as text, e.g. "1.0.0".
    std::string_view GetVersionString();
} // namespace Carbon
