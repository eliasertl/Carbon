#pragma once

#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginList. All fields are optional.
    struct ListOptions
    {
        Size Width = Size::Fill();
        /// A list scrolls, so inside a stack that fits its content it needs a fixed height.
        Size Height = Size::Fill();
        float RowHeight = 24.0f;
        /// Draws the list on a bordered control background. Turn it off for a list that fills a pane.
        bool HasBorder = true;
    };

    /// Per-call options of ListItem. All fields are optional.
    struct ListItemOptions
    {
        /// An icon before the title.
        std::string_view Icon = {};
        /// Secondary text at the trailing edge.
        std::string_view Detail = {};
        bool Disabled = false;
    };

    /// A scrolling column of rows of which the application marks any as selected.
    ///
    ///     Carbon::BeginList("files", { .Height = 200.0f });
    ///     for (int i = 0; i < count; i++)
    ///     {
    ///         Carbon::PushID(i);
    ///         if (Carbon::ListItem(names[i], i == selected))
    ///             selected = i;
    ///         Carbon::PopID();
    ///     }
    ///     Carbon::EndList();
    ///
    /// The list is one stop for Tab; with focus, the up and down arrow keys, Home and End move the selection
    /// and scroll it into view.
    void BeginList(std::string_view id, const ListOptions& options = {});
    void EndList();

    /// One row. Pass whether it is selected. Returns true when the user picks it, by click or keyboard: make it
    /// the selection (or toggle it, for a list that allows several).
    bool ListItem(std::string_view label, bool isSelected, const ListItemOptions& options = {});
} // namespace Carbon
