# Keyboard navigation

Every Carbon interface can be operated with the keyboard alone. Carbon behaves like macOS with Full Keyboard
Access turned on: every control is a Tab stop.

## Keys

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Move focus to the next / previous control, wrapping around at the ends. Held keys repeat. |
| Space | Activate the focused control: press a button, flip a toggle. |
| Enter | Activate the focused button. With no button focused, activate the default button (`IsDefault`). In a text field, submit. |
| Arrow keys | Act inside the focused control: move a slider, move the caret, move a selection. |
| Home / End | Jump to the ends: a slider's minimum and maximum, the start and end of a text field. |
| Page Up / Page Down | Scroll the scroll view under the pointer, or the outermost one. |
| Escape | Give up editing a text field. |
| Ctrl+A, C, X, V, Z, Shift+Z / Y | Select all, copy, cut, paste, undo and redo in text fields. |

The shortcut modifier is Ctrl by default. A host that prefers the Super/Command key calls
`io.SetShortcutModifier(Carbon::KeyModifiers::Super)`.

Each component's page under [Components](Components/) lists its own keys.

## Focus order

Focus moves through controls in the order your code submits them, which with stacks is reading order: leading
to trailing, top to bottom. There is nothing to configure. Disabled controls are skipped, and a control that
disappears gives up focus.

When focus moves to a control that is scrolled out of view, the enclosing scroll views scroll just far enough to
reveal it.

## The focus ring

The focused control is marked by a ring in the accent color that follows the control's squircle shape and
animates in (a fade, settling inwards onto the control; just a fade with reduced motion).

- The ring appears when focus arrives **by keyboard**. Clicking a control focuses it without showing the ring,
  so that Tab continues from where you clicked; the ring appears as soon as a key is pressed.
- **Text fields** always show the ring while they have focus, as on macOS.
- The ring hides while the host window is inactive.

Its look comes from the theme: `StyleColor::FocusRing`, `StyleVar::FocusRingWidth` and
`StyleVar::FocusRingOffset`.

## The default button

A button created with `{ .IsDefault = true }` is the default button of its view. Pressing Enter activates it
when no focused control uses the key itself. A focused button takes Enter for itself; a focused text field
submits and still lets the default button act, like a dialog on macOS. The HIG's advice applies: make the
default button the most likely choice, and never a destructive one.

## What the host does

Forward key events and text input (see [Integration](Integration.md#forwarding-input)). Send modifier keys as
ordinary key events, and do not forward the OS's key repeat: Carbon repeats held keys itself, 0.4 s after the
press and then every 50 ms.

`io.WantsKeyboard()` is true while a Carbon control has focus and `io.WantsTextInput()` while a text field is
being edited, so the host can keep those keys away from its own shortcuts.

## From code

```cpp
Carbon::SetFocus(Carbon::GetID("Name"), true);   // focus a control; true shows the ring
Carbon::ClearFocus();
bool focused = Carbon::IsFocused(id);
Carbon::ID current = Carbon::GetFocusedID();
```

After a widget call, `Carbon::IsItemFocused()` tells whether that widget has focus.

## For component authors

A custom component becomes keyboard-operable by using the same building blocks as Carbon's own:

```cpp
Carbon::Interaction interaction = Carbon::ButtonBehavior(id, rect);   // Tab stop; Space and Enter activate
if (interaction.Focused && Carbon::IsKeyPressed(Carbon::Key::RightArrow))
    ++value;                                                            // keys inside the control
Carbon::DrawFocusRing(id, rect, cornerRadius);                          // the ring, animated
```

`ButtonBehavior` and `DragBehavior` register the item for Tab navigation unless `Focusable` is false.
Components that handle input entirely on their own call `RegisterFocusable(id, rect)`.
