#pragma once

#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginTabView. All fields are optional.
    struct TabViewOptions
    {
        Size Width = Size::Fill();
        /// Fit makes the tab view as tall as the content of the selected tab.
        Size Height = Size::Fit();
        /// Space between the content area's edge and the content.
        EdgeInsets Padding = EdgeInsets(16.0f);
        /// Distance between items of the content. Defaults to the theme's Spacing.
        std::optional<float> Spacing = {};
    };

    /// Several panes of content in the same area, one visible at a time, switched with a segmented control at
    /// the top; `selected` is the index of the visible pane. Add the content of that pane, laid out like in a
    /// VStack, and call EndTabView.
    ///
    ///     Carbon::BeginTabView("settings", &tab, { "General", "Advanced" });
    ///     if (tab == 0)
    ///         BuildGeneral();
    ///     else
    ///         BuildAdvanced();
    ///     Carbon::EndTabView();
    ///
    /// The tabs are one stop for Tab; with focus, the left and right arrow keys switch between them.
    void BeginTabView(std::string_view id, int* selected, std::span<const std::string_view> tabs,
                      const TabViewOptions& options = {});
    void BeginTabView(std::string_view id, int* selected, std::initializer_list<std::string_view> tabs,
                      const TabViewOptions& options = {});
    void EndTabView();
} // namespace Carbon
