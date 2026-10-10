#pragma once

#include <span>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Input/InputEvent.h"
#include "Carbon/Input/Key.h"
#include "Carbon/Input/MouseButton.h"

namespace Carbon
{
    /// Input queries for the current frame. Valid between NewFrame and EndFrame.

    /// True when the mouse is inside the display area (a position event arrived and no leave event since).
    bool IsMousePosValid();
    /// Mouse position in points. Meaningful only when IsMousePosValid() is true.
    Vec2 GetMousePos();
    /// Mouse movement since the previous frame, in points.
    Vec2 GetMouseDelta();
    /// Scroll amount this frame, in wheel notches ("lines"). With Shift held, a vertical wheel scrolls sideways: it
    /// is reported as x.
    Vec2 GetMouseWheel();

    /// True while the button is held.
    bool IsMouseDown(MouseButton button = MouseButton::Left);
    /// True on the frame the button went down.
    bool IsMousePressed(MouseButton button = MouseButton::Left);
    /// True on the frame the button went up.
    bool IsMouseReleased(MouseButton button = MouseButton::Left);
    /// For a press this frame: 1 for a single click, 2 for a double click, and so on. 0 without a press.
    int GetMouseClickCount(MouseButton button = MouseButton::Left);
    /// Where the button was last pressed, in points. Used to measure drags.
    Vec2 GetMousePressedPos(MouseButton button = MouseButton::Left);

    /// The kind of device that drives the pointer: Touch (or Pen) after a finger moved it, Mouse otherwise. With a
    /// finger the left button is the finger being down, and there is no pointer while no finger is.
    PointerType GetPointerType();
    /// The number of fingers on the display.
    int GetTouchCount();
    /// Where the finger at `index` (0 to GetTouchCount() - 1, in the order they touched the display) is, in points.
    Vec2 GetTouchPosition(int index);

    /// True while the key is held.
    bool IsKeyDown(Key key);
    /// True on the frame the key went down and, when `repeat` is true, each time it auto-repeats while held.
    bool IsKeyPressed(Key key, bool repeat = true);
    /// True on the frame the key went up.
    bool IsKeyReleased(Key key);
    /// Modifier keys currently held.
    KeyModifiers GetKeyModifiers();
    /// True when `key` was pressed while exactly the shortcut modifier (Ctrl by default) plus `extra` is held,
    /// e.g. IsShortcutPressed(Key::C) for copy or IsShortcutPressed(Key::Z, KeyModifiers::Shift) for redo.
    bool IsShortcutPressed(Key key, KeyModifiers extra = KeyModifiers::None);

    /// Characters typed this frame, as Unicode code points.
    std::span<const char32_t> GetInputCharacters();

    /// True while the host window has focus.
    bool IsHostFocused();
} // namespace Carbon
