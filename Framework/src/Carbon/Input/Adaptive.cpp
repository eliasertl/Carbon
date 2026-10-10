#include "Carbon/Input/Adaptive.h"

#include <algorithm>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Input/AdaptiveInternal.h"

namespace Carbon
{
    namespace Internal
    {
        void UpdateAdaptiveState(Context& context)
        {
            const IO& io = context.HostIO;
            const InputState& input = context.Input;
            const PointerType pointer = input.HasPointerInput ? input.LastPointerType : io.GetDefaultPointerType();
            context.IsTouchMode = io.GetTouchModeOverride().value_or(pointer != PointerType::Mouse);
            // A display without a size yet is not narrow.
            const float width = context.DisplaySize.X;
            const SizeClass detected =
                width > 0.0f && width < CompactWidthLimit ? SizeClass::Compact : SizeClass::Regular;
            context.HorizontalSizeClass = io.GetSizeClassOverride().value_or(detected);
        }
    } // namespace Internal

    bool IsTouchMode()
    {
        return Internal::GetContext().IsTouchMode;
    }

    float GetAdaptiveRowHeight(float rowHeight)
    {
        return Internal::GetContext().IsTouchMode ? std::max(rowHeight, MinimumTouchTarget) : rowHeight;
    }

    SizeClass GetSizeClass()
    {
        return Internal::GetContext().HorizontalSizeClass;
    }
} // namespace Carbon
