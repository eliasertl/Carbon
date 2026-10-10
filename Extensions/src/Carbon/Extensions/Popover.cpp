#include "Carbon/Extensions/Popover.h"

namespace Carbon
{
    void OpenPopover(std::string_view id)
    {
        OpenOverlay(GetID(id));
    }

    void ClosePopover(std::string_view id)
    {
        CloseOverlay(GetID(id));
    }

    bool IsPopoverOpen(std::string_view id)
    {
        return IsOverlayOpen(GetID(id));
    }

    bool BeginPopover(std::string_view id, const PopoverOptions& options)
    {
        OverlayOptions overlay;
        overlay.Anchor = options.Anchor.value_or(GetItemRect());
        overlay.Placement = options.Placement;
        overlay.Alignment = Alignment::Center;
        overlay.Gap = 2.0f;
        overlay.ShowsArrow = options.ShowsArrow;
        overlay.DismissOnOutsideClick = options.DismissOnOutsideClick;
        overlay.Padding = options.Padding;
        overlay.Spacing = options.Spacing;
        overlay.Width = options.Width;
        overlay.Height = options.Height;
        overlay.PresentsAsSheetInCompactWidth = true;
        return BeginOverlay(GetID(id), overlay);
    }

    void EndPopover()
    {
        EndOverlay();
    }
} // namespace Carbon
