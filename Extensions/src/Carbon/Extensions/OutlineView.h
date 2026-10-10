#pragma once

#include <cstdint>
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
        /// The user can drag items with a Key to move them: between other items, or onto an item that can
        /// contain others. EndOutlineView reports the move for the application to apply to its tree.
        bool AllowsReordering = false;
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
        /// Identifies the item to the application in an OutlineMove: an index or ID of its own, 0 or more. In an
        /// outline view that allows reordering, only items with a key can be dragged or receive a drop.
        int64_t Key = -1;
    };

    /// Where a moved item goes, relative to the target item.
    enum class OutlineDropPosition : uint8_t
    {
        /// Before the target, among its siblings.
        Before,
        /// After the target, among its siblings.
        After,
        /// Into the target, as its last child. A Target of -1 is the root: the item becomes the last top-level one.
        Into
    };

    /// An item the user moved by dragging it, by the keys the application gave its items.
    struct OutlineMove
    {
        /// The key of the item that moved, or -1 when none did.
        int64_t Item = -1;
        /// The key of the item it goes before, after or into; -1 for the root.
        int64_t Target = -1;
        OutlineDropPosition Position = OutlineDropPosition::Into;

        /// True when an item moved.
        constexpr bool IsMoved() const { return Item >= 0; }
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
    ///
    /// With AllowsReordering, the user drags items with a Key: dropped on the upper or lower edge of an item, it
    /// goes before or after it, marked by a line; dropped onto the middle of an item that can contain others, it
    /// goes into it, marked by an outline around that item. A collapsed item expands when a drag rests on it.
    /// An item cannot be dropped into itself or anything inside it.
    ///
    ///     const Carbon::OutlineMove move = Carbon::EndOutlineView();
    ///     if (move.IsMoved())
    ///         tree.Move(move.Item, move.Target, move.Position);
    void BeginOutlineView(std::string_view id, const OutlineViewOptions& options = {});
    /// Ends the outline view. Reports the item the user moved in this frame, if any.
    OutlineMove EndOutlineView();

    /// Adds an item. Pass whether it is selected.
    OutlineItem BeginOutlineItem(std::string_view label, bool isSelected, const OutlineItemOptions& options = {});
    void EndOutlineItem();

    /// The next column of the current item, as text: call it right after BeginOutlineItem, once per column after
    /// the first.
    void OutlineCell(std::string_view text, const TableCellOptions& options = {});
} // namespace Carbon
