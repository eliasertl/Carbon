# Slider

Picks a value from a continuous or stepped range.
HIG: [Sliders](https://developer.apple.com/design/human-interface-guidelines/sliders)

```cpp
Carbon::BeginHStack({ .Spacing = 10.0f, .Width = Carbon::Size::Fill() });
    Carbon::Text("Volume");
    Carbon::Slider("Volume", &volume, 0.0f, 1.0f, { .Width = Carbon::Size::Fill() });
Carbon::EndHStack();

Carbon::Slider("Rating", &rating, 0.0f, 10.0f, { .Step = 1.0f, .ShowsTicks = true });
```

`Slider` returns `true` on frames the value changed and keeps the value inside `[min, max]`. The label identifies
the slider and is not drawn; put a `Text` next to it for a visible title.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Step` | `float` | 0 | Distance between allowed values; 0 is continuous. Also the arrow-key increment. |
| `ShowsTicks` | `bool` | `false` | A tick mark at every step |
| `ControlSize` | `ControlSize` | `Regular` | Knob of 14, 18 or 22 points |
| `Width` | `Size` | 180 points | A slider cannot fit its content, so the default is a fixed length; use `Fill` to take the rest of a row |
| `Disabled` | `bool` | `false` | |
| `Tint` | `Color` | theme's `Accent` | Color of the filled part of the track |

## Behaviour

- Dragging the knob keeps the grab point under the pointer. The drag continues when the pointer leaves the
  slider.
- Clicking the track moves the knob to the click.
- The value updates while dragging, so results can be shown live, as the HIG recommends.
- The knob grows slightly under the pointer and darkens while it is held.

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Focus the slider (the ring is drawn around the knob) |
| Right, Up | Increase by `Step`, or by 5 % of the range when continuous |
| Left, Down | Decrease |
| Home / End | Minimum / maximum |

Held arrow keys repeat.
