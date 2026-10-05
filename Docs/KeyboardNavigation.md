# Keyboard navigation

Every Carbon interface can be operated with the keyboard alone. Carbon behaves like macOS with Full Keyboard
Access turned on: every control is a Tab stop.

## Keys

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Move focus to the next / previous control, wrapping around at the ends. Held keys repeat. |
| Space | Activate the focused control: press a button, flip a toggle. |
| Enter | Activate the focused button. With no button focused, activate the default button (`IsDefault`). In a text field, submit. |
| Arrow keys | Act inside the focused control: move a slider, the caret, the selected segment, the selection of a list, the highlight of a menu, a split view's divider. |
| Home / End | Jump to the ends: a slider's minimum and maximum, the start and end of a text field. |
| Page Up / Page Down | Scroll the scroll view under the pointer, or the outermost one. |
| Escape | Close the topmost popover, menu or sheet; cancel an alert; give up editing a text field. |
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

## Overlays

A popover, menu, alert or sheet takes the keyboard while it is open (see [Overlays](Overlays.md)):

- Tab and Shift+Tab cycle through the controls **inside** it. What lies beneath cannot be reached.
- Focus is taken from whatever had it and returns there when the overlay closes.
- Enter goes to the default button inside the overlay.
- Escape closes the topmost overlay, one per key press. An alert answers Escape with its cancel button.

In menus the highlight is the keyboard focus: the up and down arrow keys move it, Enter and Space choose, the
right and left arrow keys open and close submenus.

A [menu bar](Components/MenuBar.md) opens its first menu with Alt (pressed and released on its own) or F10; then
the left and right arrow keys move between its menus. [Notifications](Components/Notification.md) never take
the keyboard.

## Groups that are one stop

Some components are a single stop for Tab and use the arrow keys inside: a segmented control (left and right), a
radio group (all four arrows), a path control (left, right, Home, End), a calendar (arrows, Page Up, Page Down,
Home, End), a stepper (up and down), a sidebar, list or table (up, down, Home, End), an outline view (also left and
right to collapse and expand). A column view has one stop per column; left and right move between them. Clicking one
of them gives it focus, so the arrow keys continue from the click.

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
Carbon::FocusNext();                              // what Tab does, on the next frame
Carbon::FocusPrevious();
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
Components that handle input entirely on their own, or that are one stop made of several parts, call
`RegisterFocusable(id, rect)`. [Custom components](CustomComponents.md#4-keyboard) has the details.
