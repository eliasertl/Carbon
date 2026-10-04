#pragma once

#include <cstdint>
#include <optional>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Input/Cursor.h"
#include "Carbon/Input/MouseButton.h"

namespace Carbon
{
    /// The building blocks every interactive component is made of: hit testing, press and drag behaviour,
    /// keyboard focus and the disabled state. Carbon's own widgets use exactly these functions.

    /// What happened to an item this frame.
    struct Interaction
    {
        /// The pointer is over the item and nothing above it.
        bool Hovered = false;
        /// The item is held down, by the mouse (with the pointer still over it) or by the keyboard.
        bool Pressed = false;
        /// The item was activated: the mouse button was released over it, or Space/Enter was pressed while it
        /// had focus.
        bool Clicked = false;
        /// The press that started this frame was a double click.
        bool DoubleClicked = false;
        /// The item has keyboard focus.
        bool Focused = false;
        /// The focus ring should be drawn: the item has focus and the user is navigating with the keyboard.
        bool FocusVisible = false;
    };

    /// Options of ButtonBehavior.
    struct ButtonBehaviorOptions
    {
        /// Takes part in Tab navigation and can be activated with Space and Enter.
        bool Focusable = true;
        /// Ignores all input.
        bool Disabled = false;
        /// Activates when the button goes down instead of when it is released (menu items, steppers).
        bool ActivateOnPress = false;
        /// While held, activates again and again like a held key (steppers).
        bool Repeat = false;
        /// Activates with Enter when no focused control uses the key: the default button of a dialog.
        bool IsDefault = false;
    };

    /// Makes `rect` behave like a button with the identity `id`: tracks hover and press, takes focus, and
    /// reports activation by mouse or keyboard. Call it once per frame for the item.
    Interaction ButtonBehavior(ID id, const Rect& rect, const ButtonBehaviorOptions& options = {});

    /// What a draggable item did this frame.
    struct DragInteraction
    {
        bool Hovered = false;
        /// The item is being dragged.
        bool Active = false;
        /// The drag started or ended this frame.
        bool Started = false;
        bool Ended = false;
        /// Pointer movement since the previous frame, and since the drag started.
        Vec2 Delta;
        Vec2 Total;
        /// Where the pointer is.
        Vec2 Position;
        bool Focused = false;
        bool FocusVisible = false;
    };

    /// Options of DragBehavior.
    struct DragBehaviorOptions
    {
        bool Focusable = true;
        bool Disabled = false;
    };

    /// Makes `rect` draggable: pressing it captures the pointer until the button is released, wherever the
    /// pointer goes. Sliders, split dividers and scroll thumbs are built on this.
    DragInteraction DragBehavior(ID id, const Rect& rect, const DragBehaviorOptions& options = {});

    /// True when the pointer is inside `rect`, inside the current clip rectangle, and not covered by an overlay.
    /// Unlike an item's Hovered state, this ignores which item is on top.
    bool IsRectHovered(const Rect& rect);

    /// Registers an item for Tab navigation at this point of the order. ButtonBehavior and DragBehavior do this
    /// for focusable items; call it yourself only for components that handle focus on their own.
    void RegisterFocusable(ID id, const Rect& rect);
    /// True when the item has keyboard focus.
    bool IsFocused(ID id);
    /// True when the item has focus and its focus ring should show.
    bool IsFocusVisible(ID id);
    /// Gives an item keyboard focus. `showRing` shows the focus ring as if the user had tabbed to it.
    void SetFocus(ID id, bool showRing = false);
    /// Removes keyboard focus from whatever has it.
    void ClearFocus();
    /// Moves focus one item forward or backward at the start of the next frame, exactly like Tab and Shift+Tab.
    /// Menus and lists use it for the arrow keys.
    void FocusNext();
    void FocusPrevious();
    /// The item with keyboard focus; invalid when nothing has it.
    ID GetFocusedID();

    /// The item the pointer is pressed on (it holds the pointer until release); invalid when none.
    ID GetActiveID();
    /// The topmost item under the pointer; invalid when none.
    ID GetHoveredID();

    /// Draws the keyboard focus ring around `rect` when the item's ring is visible. The ring follows the
    /// squircle of the control and animates in. `cornerRadius` is the control's own radius. Text fields pass
    /// `alwaysWhenFocused`: like on macOS, they show the ring whenever they have focus, not only after
    /// keyboard navigation.
    void DrawFocusRing(ID id, const Rect& rect, float cornerRadius, bool alwaysWhenFocused = false);

    /// Disables every item until the matching PopDisabled: they ignore input and are drawn dimmed. Nested calls
    /// cannot re-enable: once disabled, items stay disabled until the outermost pop.
    void PushDisabled(bool disabled = true);
    void PopDisabled();
    /// True inside a PushDisabled(true) scope.
    bool IsDisabled();

    /// Asks the host to show a cursor shape this frame (through the SetCursor callback). The default is Arrow.
    void SetCursor(Cursor cursor);

    /// Queries about the item submitted last, for code that follows a widget call:
    ///     Carbon::Button("Delete");
    ///     if (Carbon::IsItemHovered()) ...
    bool IsItemHovered();
    bool IsItemFocused();
    /// The item is being pressed or dragged.
    bool IsItemActive();
    /// The rectangle of the item submitted last.
    Rect GetItemRect();
    /// The ID of the item submitted last.
    ID GetItemID();
    /// Enter was pressed in the text field submitted last.
    bool IsItemSubmitted();

    /// Records `id` and `rect` as the item submitted last. Components call this once they know their rectangle
    /// so that Tooltip and the IsItem... queries work for them.
    void SetLastItem(ID id, const Rect& rect, const Interaction& interaction);
} // namespace Carbon
