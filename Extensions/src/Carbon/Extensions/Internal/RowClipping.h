#pragma once

#include "Carbon/Extension.h"
#include "Carbon/Extensions/RowRange.h"

namespace Carbon::Internal
{
    /// Arithmetic for rows of one height that follow each other in a vertical container, so that a long list can
    /// add only the rows that are visible and reserve the space of the others in one piece.

    /// The distance from the top of one row to the top of the next. Rows are placed on whole pixels, so this is
    /// the height plus the spacing, rounded to pixels.
    float GetRowPitch(float height, float spacing);

    /// Which of `count` rows, the first of them at the layout cursor, reach into `clip`: those, and one more on
    /// either side, because positions far down a long list are rounded.
    RowRange GetVisibleRows(int count, float height, float spacing, const Rect& clip);

    /// Reserves the space of `count` rows as one item of the current container and returns it. The next item
    /// follows where the row after them would. Does nothing for a count of zero or less.
    Rect ReserveRows(int count, float height, float spacing);
} // namespace Carbon::Internal
