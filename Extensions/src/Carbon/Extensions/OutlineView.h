#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/Table.h"

namespace Carbon
{
    /// Per-call options of BeginOutlineView. All fields are optional.
    struct OutlineViewOptions
    {
        Size Width = Size::Fill();
        /// An outline view scrolls, so inside a stack that fits its content it needs a fixed height.
        Size Height = Size::Fill();
        float RowHeight = 24.0f;
        /// Columns after the first show attributes of each item, filled with OutlineCell. The first column holds
        /// the hierarchy. Without columns the outline view is a single column without a header.
        std::span<const TableColumn> Columns = {};
        /// Tints every other row; useful when there are several columns.
        bool ShowsAlternatingRows = false;
        /// Draws the outline view on a bordered control background. Turn it off for one that fills a pane.
        bool HasBorder = true;
    };

    /// Per-call options of BeginOutlineItem. All fields are optional.
    struct OutlineItemOptions
    {
        /// An icon before the title, such as Carbon::Icons::Folder.
        std::string_view Icon = {};
        /// Replaces the accent color of the icon.
        std::optional<Color> IconTint = {};
        /// The item can contain others and has a disclosure triangle. Leaves (files) set this to false.
        bool HasChildren = true;
        /// Whether the item is expanded the first time it appears. Afterwards Carbon remembers what the user chose.
        bool IsInitiallyExpanded = false;
        bool Disabled = false;
    };

    /// What happened to an item this frame.
    struct OutlineItem
    {
        /// The item is expanded: add its children before EndOutlineItem.
        bool IsExpanded = false;
        /// The user picked the item, by click or keyboard: make it the selection.
        bool Picked = false;
        /// The user double-clicked the item or pressed Enter on it: open it.
        bool Activated = false;
    };

    /// A list of items that can contain other items, shown as an indented hierarchy with disclosure triangles:
    /// a file browser, an outline of a document.
    ///
    ///     Carbon::BeginOutlineView("files", { .Height = 300.0f });
    ///     const Carbon::OutlineItem folder = Carbon::BeginOutlineItem("Documents", selected == 1,
    ///                                                                  { .Icon = Carbon::Icons::Folder });
    ///     if (folder.Picked)
    ///         selected = 1;
    ///     if (folder.IsExpanded)
    ///     {
    ///         if (Carbon::BeginOutlineItem("Report.pdf", selected == 2, { .HasChildren = false }).Picked)
    ///             selected = 2;
    ///         Carbon::EndOutlineItem();
    ///     }
    ///     Carbon::EndOutlineItem();
    ///     Carbon::EndOutlineView();
    ///
    /// Every BeginOutlineItem needs its EndOutlineItem, after the item's children. Labels only need to be unique
    /// among siblings: each item pushes its ID for its children. The application owns the selection; Carbon
    /// remembers which items are expanded.
    ///
    /// The outline view is one stop for Tab. With focus, the up and down arrow keys move the selection, the right
    /// arrow expands an item or moves to its first child, the left arrow collapses it or moves to its parent.
    /// With Alt held, or when Alt-clicking a disclosure triangle, everything nested inside expands or collapses
    /// too.
    void BeginOutlineView(std::string_view id, const OutlineViewOptions& options = {});
    void EndOutlineView();

    /// Adds an item. Pass whether it is selected.
    OutlineItem BeginOutlineItem(std::string_view label, bool isSelected, const OutlineItemOptions& options = {});
    void EndOutlineItem();

    /// The next column of the current item, as text: call it right after BeginOutlineItem, once per column after
    /// the first.
    void OutlineCell(std::string_view text, const TableCellOptions& options = {});
} // namespace Carbon
