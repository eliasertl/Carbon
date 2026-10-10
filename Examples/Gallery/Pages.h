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

    /// For screenshots of single sections (--section): the keys of the sections to capture, separated by commas.
    /// A key is a section's title in lower case without spaces or punctuation ("Text fields" is "textfields").
    void SetCapturedSections(std::string_view keys);
    /// The area of the boxes of the captured sections, as drawn during this frame; empty when none was drawn.
    Carbon::Rect GetCapturedArea();
    /// Called at the start of each frame.
    void ResetCapturedArea();

    // Core components (CorePages.cpp).
    void TypographyPage(const GalleryState& state);
    void IconsPage();
    void ButtonsPage(GalleryState& state);
    void TogglesPage(GalleryState& state);
    void SlidersPage(GalleryState& state);
    void TextFieldsPage(GalleryState& state);
    void ImagesPage(GalleryState& state);
    void LayoutPage(GalleryState& state);

    // Components of CarbonExtensions (ExtensionPages.cpp).
    void SelectionPage(GalleryState& state);
    void DatesPage(GalleryState& state);
    void MenusPage(GalleryState& state);
    void ToolbarsPage(GalleryState& state);
    void DialogsPage(GalleryState& state);
    void NotificationsPage(GalleryState& state);
    void ProgressPage(GalleryState& state);
    void ListsPage(GalleryState& state);
    void HierarchiesPage(GalleryState& state);
    // Drag and drop (DragDropPage.cpp).
    void DragDropPage(GalleryState& state);
    void NavigationPage(GalleryState& state);
    void ChartsPage(GalleryState& state);
#if defined(CARBON_GALLERY_HAVE_REFLECTION)
    // CarbonReflection (ReflectionPage.cpp).
    void ReflectionPage();
#endif

    /// Draws the notifications posted from any page and reports what the user did with them.
    void ShowGalleryNotifications(GalleryState& state);
} // namespace Gallery
