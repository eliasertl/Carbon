#pragma once

#include <string_view>

#include "GalleryState.h"

namespace Gallery
{
    /// Width of the column of row labels in a section.
    inline constexpr float LabelColumn = 150.0f;

    /// A titled group: a headline above a rounded box, like a section of System Settings.
    void BeginSection(std::string_view title, std::string_view description = {});
    void EndSection();

    /// One row of a section: a secondary label in a fixed column, then the content.
    void BeginRow(std::string_view label);
    void EndRow();

    // Core components (CorePages.cpp).
    void TypographyPage();
    void IconsPage();
    void ButtonsPage(GalleryState& state);
    void TogglesPage(GalleryState& state);
    void SlidersPage(GalleryState& state);
    void TextFieldsPage(GalleryState& state);
    void ImagesPage(GalleryState& state);
    void LayoutPage();

    // Components of CarbonExtensions (ExtensionPages.cpp).
    void SelectionPage(GalleryState& state);
    void MenusPage(GalleryState& state);
    void DialogsPage(GalleryState& state);
    void ProgressPage(GalleryState& state);
    void ListsPage(GalleryState& state);
    void NavigationPage(GalleryState& state);
    void ChartsPage(GalleryState& state);
} // namespace Gallery
