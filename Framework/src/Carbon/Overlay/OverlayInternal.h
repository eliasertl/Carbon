#pragma once

#include <cstdint>
#include <vector>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Draw/DrawTypes.h"
#include "Carbon/Overlay/Overlay.h"

namespace Carbon
{
    struct Context;
}

namespace Carbon::Internal
{
    /// An overlay that is open. Its position in OverlayState::Open is its depth: the sub-layer it draws in.
    struct OpenOverlayEntry
    {
        ID Id;
        /// Set by the first BeginOverlay; until then the overlay has no options and no size.
        bool IsStarted = false;
        /// The overlay holds the pointer and the keyboard: it is modal or dismisses on outside clicks.
        bool Captures = false;
        /// What had focus when the overlay took the keyboard; it gets focus back when the overlay closes.
        ID PreviousFocus;
        bool WasFocusVisible = false;
        uint64_t OpenedFrame = 0;
        /// The last frame BeginOverlay was called for it.
        uint64_t LastFrame = 0;
        /// Where the overlay was drawn last. Its size places it during the next frame.
        Rect Bounds;
    };

    /// An overlay whose content is being built: between BeginOverlay and EndOverlay.
    struct OverlayBuild
    {
        ID Id;
        bool Captures = false;
        DeferredShape Shadow;
        DeferredShape Background;
        DeferredShape Border;
        float Radius = 0.0f;
        /// Opacity of the fade-in, for what is drawn after the content.
        float Opacity = 1.0f;
        bool ShowsArrow = false;
        Rect Anchor;
        /// The side the overlay ended up on, after flipping.
        OverlayPlacement Placement = OverlayPlacement::Below;
    };

    /// The overlay state of one context.
    struct OverlayState
    {
        std::vector<OpenOverlayEntry> Open;
        std::vector<OverlayBuild> Building;
        /// Escape closes one overlay per press.
        bool IsEscapeConsumed = false;
    };

    /// Called by NewFrame.
    void BeginOverlays(Context& context);
    /// Called by EndFrame: closes the overlays that were not submitted this frame.
    void EndOverlays(Context& context);
    /// Unwinds an overlay that was begun but not ended, after the mistake has been reported.
    void AbandonOverlay(Context& context);

    /// The overlay that holds the keyboard: the topmost open one that captures input. Invalid when none does.
    ID GetActiveFocusScope(const Context& context);
    /// The capturing overlay whose content is being built right now. Invalid outside of one.
    ID GetCurrentFocusScope(const Context& context);
    /// True when the items being built right now may use the keyboard.
    bool IsInActiveFocusScope(const Context& context);
    /// True when an overlay above the current layer covers the pointer, or holds it altogether.
    bool IsPointerBlockedByOverlay(const Context& context);
    /// The same for any point.
    bool IsPointBlockedByOverlay(const Context& context, Vec2 point);
} // namespace Carbon::Internal
