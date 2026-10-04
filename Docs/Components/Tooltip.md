# Tooltip

A short description that appears when the pointer rests on a control.
HIG: [Offering help](https://developer.apple.com/design/human-interface-guidelines/offering-help)

```cpp
Carbon::Button("##restore", { .Icon = Carbon::Icons::ArrowCounterClockwise });
Carbon::Tooltip("Restore default settings");
```

`Tooltip` attaches to the component submitted just before it. It works after any component, including `Text`
and `Image`.

## Behaviour

- Appears after the pointer has rested on the item for 0.7 seconds (`Carbon::TooltipDelay`), fading in.
- Sits just below the pointer, stays where it appeared while the pointer moves within the item, and is kept
  inside the display.
- Disappears when the pointer leaves the item or a mouse button is pressed.
- Is drawn on the topmost layer, above popovers and menus.

There are no options. The look comes from the theme (`OverlayBackground`, `OverlayBorder`, `Shadow`).

## Keyboard

Tooltips are a pointer feature and have no keyboard behaviour. Do not put information in a tooltip that a
keyboard user needs.

## Guidance from the HIG

- Describe what the control does, starting with a verb: "Restore default settings", "Add or remove a language".
- Use a sentence fragment in sentence case, without ending punctuation, of at most 60 to 75 characters.
- Do not repeat the control's name.
