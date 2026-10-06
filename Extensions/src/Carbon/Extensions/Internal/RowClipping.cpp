#include "Carbon/Extensions/Internal/RowClipping.h"

#include <algorithm>
#include <cmath>

namespace Carbon::Internal
{
    float GetRowPitch(float height, float spacing)
    {
        return GetContentScale().Snap(height + spacing);
    }

    RowRange GetVisibleRows(int count, float height, float spacing, const Rect& clip)
    {
        RowRange range;
        range.End = std::max(count, 0);
        const float pitch = GetRowPitch(height, spacing);
        if (count <= 0 || pitch <= 0.0f)
            return range;

        const float top = GetContentScale().Snap(GetCursorPos().Y);
        const float limit = static_cast<float>(count);
        const float first = std::clamp(std::floor((clip.Y - top) / pitch) - 1.0f, 0.0f, limit);
        const float end = std::clamp(std::ceil((clip.GetBottom() - top) / pitch) + 1.0f, first, limit);
        range.First = static_cast<int>(first);
        range.End = static_cast<int>(end);
        return range;
    }

    Rect ReserveRows(int count, float height, float spacing)
    {
        if (count <= 0)
            return Rect();
        ItemOptions item;
        item.Width = Size::Fill();
        return AllocateItem(Vec2(0.0f, GetRowPitch(height, spacing) * static_cast<float>(count) - spacing), item);
    }
} // namespace Carbon::Internal
