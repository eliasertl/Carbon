#include "Carbon/Extensions/Sheet.h"

namespace Carbon
{
    namespace
    {
        constexpr float SheetCornerRadius = 12.0f;
    }

    void OpenSheet(std::string_view id)
    {
        OpenOverlay(GetID(id));
    }

    void CloseSheet(std::string_view id)
    {
        CloseOverlay(GetID(id));
    }

    bool IsSheetOpen(std::string_view id)
    {
        return IsOverlayOpen(GetID(id));
    }

    bool BeginSheet(std::string_view id, const SheetOptions& options)
    {
        OverlayOptions overlay;
        overlay.Placement = OverlayPlacement::Center;
        overlay.IsModal = true;
        overlay.HasScrim = true;
        overlay.DismissOnOutsideClick = false;
        overlay.DismissOnEscape = options.DismissOnEscape;
        overlay.Padding = options.Padding;
        overlay.Spacing = options.Spacing;
        overlay.Width = options.Width;
        overlay.Height = options.Height;
        overlay.CornerRadius = SheetCornerRadius;
        overlay.PresentsAsSheetInCompactWidth = true;
        return BeginOverlay(GetID(id), overlay);
    }

    void EndSheet()
    {
        EndOverlay();
    }

    void CloseCurrentSheet()
    {
        CloseCurrentOverlay();
    }
} // namespace Carbon
