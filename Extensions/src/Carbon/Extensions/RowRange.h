#pragma once

namespace Carbon
{
    /// The rows of a long list that have to be submitted in a frame: `First` up to, but not including, `End`.
    /// Returned by ClipTableRows, ClipListItems and ClipColumnViewItems, which let a list of any length cost
    /// only what its visible rows cost.
    struct RowRange
    {
        int First = 0;
        int End = 0;
    };
} // namespace Carbon
