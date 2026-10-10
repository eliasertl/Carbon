#pragma once

#include <algorithm>
#include <string_view>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/RowRange.h"

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
        /// The user can drag an item to another place: the other items make room, a line marks where it goes,
        /// and EndList reports the move for the application to apply. Give items IDs that follow them, not their
        /// position (PushID of a key, not of the index), so that they slide into their new places.
        bool AllowsReordering = false;
    };

    /// An item the user moved by dragging it. Apply it to the items with ApplyListMove.
    struct ListMove
    {
        /// The index of the item that moved, or -1 when none did.
        int From = -1;
        /// The index the item has after the move: where it ends up once it has been taken out and put back in.
        int To = -1;

        /// True when an item moved.
        constexpr bool IsMoved() const { return From >= 0 && To >= 0 && From != To; }
    };

    /// Moves the element at move.From to move.To, shifting the elements in between: the application's side of a
    /// reorder. Works for any container with random-access iterators (std::vector, std::array, std::deque).
    template <typename Container>
    void ApplyListMove(Container& items, const ListMove& move)
    {
        const auto count = static_cast<int>(std::end(items) - std::begin(items));
        if (!move.IsMoved() || move.From >= count || move.To >= count)
            return;
        const auto from = std::begin(items) + move.From;
        const auto to = std::begin(items) + move.To;
        if (move.From < move.To)
            std::rotate(from, from + 1, to + 1);
        else
            std::rotate(to, from, from + 1);
    }

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
    ///
    /// A list with AllowsReordering lets the user drag an item to another place:
    ///
    ///     Carbon::BeginList("songs", { .Height = 200.0f, .AllowsReordering = true });
    ///     for (const Song& song : songs)
    ///     {
    ///         Carbon::PushID(song.Id);
    ///         ...
    ///         Carbon::PopID();
    ///     }
    ///     const Carbon::ListMove move = Carbon::EndList();
    ///     Carbon::ApplyListMove(songs, move);
    void BeginList(std::string_view id, const ListOptions& options = {});
    /// Ends the list. Reports the item the user moved in this frame, if any.
    ListMove EndList();

    /// For a list with many items: declares that the list has `count` items and returns the ones to submit in
    /// this frame, which are the visible ones and, after the arrow keys moved the selection, the item they moved
    /// to. The list reserves the space of all the others, so a frame costs the same however long the list is:
    ///
    ///     Carbon::BeginList("files", { .Height = 200.0f });
    ///     const Carbon::RowRange items = Carbon::ClipListItems(count, selected);
    ///     for (int i = items.First; i < items.End; i++)
    ///         if (Carbon::ListItem(names[i], i == selected))
    ///             selected = i;
    ///     Carbon::EndList();
    ///
    /// Call it once, right after BeginList, and submit exactly the items of the range, in order. `selectedItem`
    /// is the index of the selected item, or -1: the keyboard moves the selection from there even while that item
    /// is not among the submitted ones. The items that are left out count as enabled.
    RowRange ClipListItems(int count, int selectedItem = -1);
    /// One row. Pass whether it is selected. Returns true when the user picks it, by click or keyboard: make it
    /// the selection (or toggle it, for a list that allows several).
    bool ListItem(std::string_view label, bool isSelected, const ListItemOptions& options = {});
} // namespace Carbon
