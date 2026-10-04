#pragma once

#include <cstdint>
#include <optional>

#include "Carbon/Core/EdgeInsets.h"
#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Layout/Size.h"

namespace Carbon
{
    /// Overlays are surfaces that float above the interface: popovers, menus, alerts and sheets are all built on
    /// the functions in this header. An overlay draws in its own layer above everything else, and while it is
    /// open it can take the pointer and the keyboard away from what lies beneath.
    ///
    /// Open overlays form a stack. One opened later draws above one opened earlier, and closing an overlay also
    /// closes everything opened after it.
    ///
    ///     if (Carbon::Button("Options"))
    ///         Carbon::OpenOverlay(Carbon::GetID("options"));
    ///     if (Carbon::BeginOverlay(Carbon::GetID("options"), { .Anchor = Carbon::GetItemRect() }))
    ///     {
    ///         Carbon::Text("Content, laid out like in a VStack");
    ///         Carbon::EndOverlay();
    ///     }

    /// Where an overlay sits.
    enum class OverlayPlacement : uint8_t
    {
        /// Next to the anchor rectangle, on the given side. When there is not enough room on that side and more
        /// on the opposite one, the overlay flips over.
        Below,
        Above,
        Trailing,
        Leading,
        /// In the middle of the display; the anchor is ignored (alerts).
        Center,
        /// Centered horizontally at the top of the display, `Gap` below its edge; the anchor is ignored (sheets).
        Top
    };

    /// Per-call options of BeginOverlay.
    struct OverlayOptions
    {
        /// The rectangle the overlay belongs to, usually the control that opened it. A rectangle without size
        /// places the overlay at a point.
        Rect Anchor = {};
        OverlayPlacement Placement = OverlayPlacement::Below;
        /// Where the overlay sits along the anchor's edge: aligned with its start, centered, or with its end.
        Carbon::Alignment Alignment = Carbon::Alignment::Leading;
        /// Distance between the anchor and the overlay.
        float Gap = 4.0f;
        /// Blocks the interface beneath: clicks outside the overlay do nothing.
        bool IsModal = false;
        /// Dims everything beneath the overlay.
        bool HasScrim = false;
        /// A click outside closes the overlay. The click is used up: it does not reach what lies beneath.
        bool DismissOnOutsideClick = true;
        /// Escape closes the overlay while it is the topmost one.
        bool DismissOnEscape = true;
        /// Draws an arrow that points at the anchor (popovers).
        bool ShowsArrow = false;
        /// Space between the overlay's edge and its content.
        EdgeInsets Padding = EdgeInsets(12.0f);
        /// Distance between items. Defaults to the theme's Spacing.
        std::optional<float> Spacing = {};
        /// Fit sizes the overlay to its content; Fill takes the display's full extent.
        Size Width = Size::Fit();
        Size Height = Size::Fit();
        /// Where items sit horizontally.
        Carbon::Alignment ContentAlignment = Carbon::Alignment::Leading;
        /// Defaults to the theme's OverlayCornerRadius.
        std::optional<float> CornerRadius = {};
    };

    /// Opens the overlay `id` above the ones that are already open. Nothing shows until BeginOverlay is called
    /// for it; an open overlay whose BeginOverlay is not called during a frame closes by itself.
    void OpenOverlay(ID id);
    /// Closes an overlay and every overlay opened after it. Does nothing when it is not open.
    void CloseOverlay(ID id);
    /// Closes the overlay whose content is being built: what a menu item does when it is chosen.
    void CloseCurrentOverlay();
    /// True between OpenOverlay and the overlay's dismissal.
    bool IsOverlayOpen(ID id);
    /// True while any overlay is open.
    bool IsAnyOverlayOpen();

    /// Starts the content of an overlay. Returns false, and must not be followed by EndOverlay, when the overlay
    /// is closed. Items are laid out top to bottom like in a VStack, and `id` is pushed on the ID stack.
    ///
    /// An overlay that is modal or dismisses on outside clicks holds the pointer and the keyboard while it is
    /// the topmost one: nothing beneath reacts, Tab cycles through the overlay's own items, and focus returns to
    /// where it was when the overlay closes.
    bool BeginOverlay(ID id, const OverlayOptions& options = {});
    void EndOverlay();
} // namespace Carbon
