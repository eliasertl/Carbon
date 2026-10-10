#pragma once

#include <cstdint>
#include <string_view>

namespace WebApp
{
    /// What the user picked on the start screen.
    enum class StartChoice : uint8_t
    {
        None,
        Gallery,
        Docs
    };

    /// Builds the start screen for one frame, filling the display: Carbon's name, two cards that open the Gallery
    /// and the documentation, and a link to the repository at `repositoryUrl`. `isDark` is the appearance, which
    /// the toggle in the corner changes.
    StartChoice BuildStartScreen(bool& isDark, std::string_view repositoryUrl);
} // namespace WebApp
