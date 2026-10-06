#pragma once

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
    };

    /// What happened to a row this frame.
    struct SelectionRow
    {
        Rect Bounds;
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

    /// Starts a selection list. Lists do not nest.
    void BeginSelectionList(std::string_view id, const SelectionListDescription& description);
    void EndSelectionList();
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
} // namespace Carbon::Internal
