#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/RowRange.h"

namespace Carbon
{
    /// Per-call options of BeginColumnView. All fields are optional.
    struct ColumnViewOptions
    {
        Size Width = Size::Fill();
        /// A column view scrolls, so inside a stack that fits its content it needs a fixed height.
        Size Height = Size::Fill();
        /// Width of a column until the user resizes it.
        float ColumnWidth = 200.0f;
        float MinColumnWidth = 120.0f;
        /// Width of the preview column (BeginColumnViewPreview).
        float PreviewWidth = 260.0f;
        float RowHeight = 24.0f;
        /// Draws the column view on a bordered control background. Turn it off for one that fills a pane.
        bool HasBorder = true;
    };

    /// Per-call options of ColumnViewItem. All fields are optional.
    struct ColumnViewItemOptions
    {
        /// An icon before the title, such as Carbon::Icons::Folder.
        std::string_view Icon = {};
        /// Replaces the accent color of the icon.
        std::optional<Color> IconTint = {};
        /// The item contains others: it shows a chevron, and picking it should open a column with its children.
        bool HasChildren = false;
        bool Disabled = false;
    };

    /// A hierarchy shown as columns, one per level, like the column view of a file browser: picking an item in
    /// one column shows its children in the next. The application owns the path of selected items and decides
    /// which columns to show.
    ///
    ///     Carbon::BeginColumnView("browser", { .Height = 260.0f });
    ///     Carbon::BeginColumnViewColumn();
    ///     for (int i = 0; i < int(root.size()); i++)
    ///         if (Carbon::ColumnViewItem(root[i].Name, path[0] == i, { .HasChildren = root[i].IsFolder }))
    ///             path = { i };                                  // picking an item shortens the path
    ///     Carbon::EndColumnViewColumn();
    ///     // ... one column per selected folder in the path ...
    ///     Carbon::BeginColumnViewPreview();                      // details of a selected file
    ///     Carbon::Text(file.Name);
    ///     Carbon::EndColumnViewPreview();
    ///     Carbon::EndColumnView();
    ///
    /// Each column is a stop for Tab. With focus, the up and down arrow keys move the selection within a column,
    /// the right arrow moves into the next column and the left arrow back to the previous one. Columns can be
    /// resized by dragging the line between them, and the view scrolls to show a column that appears.
    void BeginColumnView(std::string_view id, const ColumnViewOptions& options = {});
    void EndColumnView();

    /// Starts the next column; add its items with ColumnViewItem.
    void BeginColumnViewColumn();
    void EndColumnViewColumn();
    /// For a column with many items: declares that the current column has `count` items and returns the ones to
    /// submit in this frame, which are the visible ones and, after the arrow keys moved the selection, the item
    /// they moved to. The column reserves the space of all the others:
    ///
    ///     Carbon::BeginColumnViewColumn();
    ///     const Carbon::RowRange items = Carbon::ClipColumnViewItems(count, selected, isSelectedAFolder);
    ///     for (int i = items.First; i < items.End; i++)
    ///         if (Carbon::ColumnViewItem(names[i], i == selected, { .HasChildren = isFolder[i] }))
    ///             selected = i;
    ///     Carbon::EndColumnViewColumn();
    ///
    /// Call it once, right after BeginColumnViewColumn, and submit exactly the items of the range, in order.
    /// `selectedItem` is the index of the column's selected item, or -1, and `selectedHasChildren` whether that
    /// item contains others: the keyboard moves on from there even while the item is not among the submitted
    /// ones. The items that are left out count as enabled.
    RowRange ClipColumnViewItems(int count, int selectedItem = -1, bool selectedHasChildren = false);
    /// An item of the current column. Pass whether it is selected. Returns true when the user picks it, by click
    /// or keyboard.
    bool ColumnViewItem(std::string_view label, bool isSelected, const ColumnViewItemOptions& options = {});

    /// A column after the last one, with information about the selected item when it has no children. Its
    /// content is laid out like in a VStack.
    void BeginColumnViewPreview();
    void EndColumnViewPreview();
} // namespace Carbon
