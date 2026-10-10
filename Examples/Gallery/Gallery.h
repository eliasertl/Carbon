#pragma once

#include <optional>
#include <string_view>

#include "GalleryState.h"
#include "GraphicsDevice.h"

namespace Gallery
{
    /// The ID of the Gallery's navigation split view: ShowNavigationDetail(NavigationID, false) returns from a page to
    /// the list of pages on a phone.
    inline constexpr std::string_view NavigationID = "gallery";

    /// Options of BuildGallery. All fields are optional.
    struct GalleryOptions
    {
        /// Shows a back button before the page title, for an application that opened the Gallery from somewhere
        /// else (the web app's start screen).
        bool HasBackButton = false;
    };

    /// Builds the whole Gallery for one frame: the sidebar of pages, the header with the appearance toggles, and
    /// the current page in its own scroll view. Call it between NewFrame and EndFrame. Returns true when the back
    /// button was clicked.
    bool BuildGallery(GalleryState& state, const GalleryOptions& options = {});

    /// The page whose key (as --page takes it: "buttons", "selection", ...) is `key`; empty when there is none.
    std::optional<Page> FindPage(std::string_view key);
    /// The key of a page.
    std::string_view GetPageKey(Page page);

    /// A small procedural texture of the host's own, which the Images page shows.
    Carbon::TextureID CreateArtwork(Example::GraphicsDevice& device);

    /// For screenshots of single sections (--section): points between the top of the page and the first captured
    /// section, around the captured area, and the frame in which --show takes effect (scripted pointer input
    /// starts at frame 6).
    inline constexpr float SectionScreenshotHeadroom = 40.0f;
    inline constexpr float SectionScreenshotMargin = 6.0f;
    inline constexpr int SectionScreenshotShowFrame = 4;
} // namespace Gallery
