#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Extension.h"

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
        Carbon::Interaction Interaction;
    };

    /// Starts a selection list. Lists do not nest.
    void BeginSelectionList(std::string_view id, const SelectionListDescription& description);
    void EndSelectionList();
    /// Adds a row that spans the list's width. Disabled rows cannot be picked and are skipped by the keyboard.
    SelectionRow SelectionListRow(ID id, float height, bool isSelected, bool isDisabled);
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
