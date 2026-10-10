#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/RowRange.h"

namespace Carbon::Internal
{
    /// What Sidebar, List and Table have in common: a scrolling column of rows of which one can be selected,
    /// with the selection drawn as a rounded highlight behind the row. The list is one stop for Tab; while it
    /// has focus the arrow keys, Home and End move the selection and the highlight takes the selection color.
    ///
    /// Carbon does not own the selection. A row reports that it was picked, by mouse or by keyboard, and the
    /// application decides what is selected on the next frame.
    /// How the rows of a selection list can be moved by drag and drop.
    enum class SelectionListReordering : uint8_t
    {
        None,
        /// A row is dragged to another place among the others: they make room where it would go, an insertion
        /// line marks the place, and EndSelectionList reports the move (List).
        Gap,
        /// Rows can be dragged, and the component finds where they go itself (OutlineView).
        Custom
    };

    struct SelectionListDescription
    {
        ScrollViewOptions Scroll;
        /// Fills the list's area behind the rows.
        std::optional<Color> Background = {};
        float BackgroundRadius = 0.0f;
        /// Outlines the list's area and shows the focus ring around it.
        bool HasBorder = false;
        /// Draws a hairline along the trailing edge (sidebars).
        bool HasTrailingSeparator = false;
        float RowRadius = 5.0f;
        /// The highlight slides to a newly selected row instead of jumping.
        bool AnimatesHighlight = false;
        SelectionListReordering Reordering = SelectionListReordering::None;
    };

    /// What happened to a row this frame.
    struct SelectionRow
    {
        ID Id;
        /// Where the row is drawn. While rows make room for a dragged one, or slide into their places after a
        /// drop, this is away from Layout.
        Rect Bounds;
        /// Where the layout put the row, which is where it takes the pointer.
        Rect Layout;
        /// The index of the row among all rows of the list, added or skipped, disabled or not.
        int Index = -1;
        /// The row is being dragged to another place.
        bool IsDragged = false;
        /// The row was picked: clicked, or reached with the arrow keys.
        bool Clicked = false;
        /// The row is selected and the list has focus: its content is drawn in the on-accent color.
        bool IsEmphasized = false;
        /// Position among the rows that can be picked (enabled rows), or -1 for a disabled row.
        int Ordinal = -1;
        /// False for a row that is scrolled out of the list's visible area. It takes its space and can report
        /// being picked by the keyboard, but it has no pointer interaction and needs no drawing.
        bool IsVisible = true;
        Carbon::Interaction Interaction;
    };

    /// The payload of a row that is dragged: the list it belongs to, its index, and the key the component gave
    /// it (OutlineView identifies items by the application's keys).
    struct RowDragPayload
    {
        ID List;
        int Index = -1;
        int64_t Key = -1;
    };
    inline constexpr std::string_view RowDragPayloadType = "Carbon.ListRow";

    /// A row moved by drag and drop, in a list with Gap reordering: from its index to the one it has after the
    /// move. From is -1 when nothing moved.
    struct RowMove
    {
        int From = -1;
        int To = -1;
    };

    /// Starts a selection list. Lists do not nest.
    void BeginSelectionList(std::string_view id, const SelectionListDescription& description);
    /// Ends the list and reports a row the user moved during this frame.
    RowMove EndSelectionList();
    /// Adds a row that spans the list's width. Disabled rows cannot be picked and are skipped by the keyboard.
    /// A row outside the visible area costs little: it is neither hit-tested nor drawn (see SelectionRow).
    SelectionRow SelectionListRow(ID id, float height, bool isSelected, bool isDisabled);
    /// Whether the next row, `height` points tall, will be inside the visible area. A component whose row ID is
    /// the hash of a label asks first and passes an invalid ID for a row that is not: it needs none.
    bool IsNextSelectionListRowVisible(float height);
    /// For many rows of one height: declares `count` rows at once and returns the ones to add with
    /// SelectionListRow, in order. Those are the visible rows and the row the keyboard is moving to. The space
    /// of the rows before them is reserved here, that of the rows after them when the list ends. `selected` is
    /// the index of the selected row among the `count`, or -1; the selection is tracked even when that row is
    /// not added. The rows that are left out count as enabled.
    RowRange ClipSelectionListRows(int count, float height, int selected);
    /// The area of the list being built, without its padding.
    Rect GetSelectionListContentRect();
    /// The ID of the list being built: the one that takes focus.
    ID GetSelectionListID();
    /// True while the list being built has keyboard focus.
    bool IsSelectionListFocused();
    /// Makes the row at `ordinal` report being picked during the next frame and scrolls it into view, as if the
    /// arrow keys had moved there. Outline views use it to move to a parent row.
    void RequestSelectionListPick(int ordinal);

    /// Makes a row of a list that allows reordering a drag source. Returns true while it is dragged: then add
    /// the preview, as after BeginDragSource, and call EndSelectionListRowDrag. Works for rows outside the
    /// visible area too, so that the preview stays while the list scrolls.
    bool BeginSelectionListRowDrag(const SelectionRow& row, int64_t key = -1);
    void EndSelectionListRowDrag();
    /// The row of the list being built that is dragged; Index is -1 when none is.
    RowDragPayload GetSelectionListDrag();
    /// True while a row of the list being built is dragged. Rows outside the visible area need their IDs
    /// meanwhile, so that the dragged one is recognized.
    bool IsSelectionListDragging();
    /// The visible area of the list being built, with its padding.
    Rect GetSelectionListViewport();
} // namespace Carbon::Internal
