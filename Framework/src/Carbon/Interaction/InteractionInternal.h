#pragma once

#include <cstdint>
#include <vector>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Input/Cursor.h"
#include "Carbon/Interaction/Interaction.h"

namespace Carbon
{
    struct Context;
    class IO;
} // namespace Carbon

namespace Carbon::Internal
{
    /// The interaction state of one context: which item is hovered, pressed and focused.
    struct InteractionState
    {
        /// Tells the host, through its IO object, whether Carbon is using the mouse and the keyboard.
        void PublishTo(IO& io) const;

        /// The topmost item under the pointer, as found during the previous frame. An item counts as hovered
        /// only when it is this one, which is what lets a button inside a clickable row win over the row.
        ID HoveredID;
        /// The item claiming the pointer during the current frame; becomes HoveredID at the end of it. A later
        /// claim replaces an earlier one unless the earlier one is on a higher layer.
        ID HoverCandidate;
        uint32_t HoverCandidateLayer = 0;
        /// Squared distance from the pointer to the candidate's own rectangle (0 inside it).
        float HoverCandidateDistance = 0.0f;

        /// The hit areas of the items that took part in hit testing this frame, for tests that check touch
        /// targets; recorded only while IsRecordingHitRects is set.
        struct HitRect
        {
            ID Id;
            Rect Area;
        };
        bool IsRecordingHitRects = false;
        std::vector<HitRect> HitRects;

        /// The item holding the pointer between press and release.
        ID ActiveID;
        /// Whether ActiveID was touched this frame; an active item that disappears releases the pointer.
        bool IsActiveAlive = false;
        /// ActiveID drags on its own (DragBehavior, a drag, a gesture): a finger that moves does not scroll
        /// instead.
        bool IsActiveDrag = false;

        ID FocusedID;
        /// The focus ring shows only after keyboard navigation.
        bool IsFocusVisible = false;
        /// The focused item registered itself this frame; if it does not, focus is dropped.
        bool IsFocusedAlive = false;
        /// Focus moved by keyboard this frame: the item should be scrolled into view.
        bool DidFocusMove = false;
        /// The frame focus was last assigned; an item focused from code gets a frame to show up.
        uint64_t FocusSetFrame = 0;
        /// The item whose focus ring is showing or fading out.
        ID FocusRingID;

        /// A focusable item and the overlay it belongs to. While an overlay holds the keyboard, Tab moves only
        /// through the items of that overlay.
        struct FocusEntry
        {
            ID Id;
            ID Scope;
        };
        /// Focusable items in submission order: this frame's (being built) and last frame's (used for Tab).
        std::vector<FocusEntry> FocusOrder;
        std::vector<FocusEntry> PreviousFocusOrder;
        /// A step requested with FocusNext or FocusPrevious, carried out at the start of the next frame.
        int PendingFocusMove = 0;
        /// An item that uses Tab itself while focused (a text area that inserts tabs) claims the key during a
        /// frame; the claim decides at the start of the next frame whether Tab moves the focus. See TakeTabKey.
        ID TabTakerThisFrame;
        ID TabTaker;

        /// The default button seen this frame, and the one to activate because Enter was pressed last frame
        /// without any focused control using it.
        ID DefaultButton;
        ID PendingDefaultActivation;
        bool IsEnterConsumed = false;

        /// One entry per PushDisabled; DisabledDepth counts the entries that are true.
        std::vector<bool> DisabledStack;
        int DisabledDepth = 0;

        Cursor RequestedCursor = Cursor::Arrow;
        Cursor ShownCursor = Cursor::Arrow;
        /// A text field is being edited this frame.
        bool IsTextInputActive = false;
        /// The caret of the text field being edited, in points, for the host's input method; see IO::GetCaretRect.
        Rect TextInputCaretRect;
        /// Carbon ended a composition this frame that the host's input method still holds.
        bool IsCompositionCancelRequested = false;

        struct LastItemData
        {
            ID Id;
            Rect Bounds;
            bool Hovered = false;
            bool Focused = false;
            bool Active = false;
            bool Submitted = false;
        };
        LastItemData LastItem;
    };

    /// Called by NewFrame after input was applied: resolves Tab navigation and resets per-frame state.
    void BeginInteraction(Context& context);
    /// Called by EndFrame: publishes the hovered item, drops stale focus and reports to the host.
    void EndInteraction(Context& context);

    /// Hover test shared by the behaviours: true when the pointer is over `rect` and `id` is the topmost item
    /// there. Also claims the pointer for `id` for the next frame.
    bool UpdateHover(Context& context, ID id, const Rect& rect);

    /// Called every frame by a focused item that uses Tab itself. While it stays focused, Tab is not used for
    /// navigation; Ctrl+Tab moves the focus forward instead, and Shift+Tab still moves it back.
    void TakeTabKey(Context& context, ID id);
} // namespace Carbon::Internal
