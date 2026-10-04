# PopUpButton

Shows one of several mutually exclusive values and opens a menu to pick another.
HIG: [Pop-up buttons](https://developer.apple.com/design/human-interface-guidelines/pop-up-buttons)
Library: CarbonExtensions, `#include <Carbon/Extensions/PopUpButton.h>`

```cpp
int sortOrder = 0;
if (Carbon::PopUpButton("Sort", &sortOrder, { "Name", "Date Modified", "Size", "Kind" }))
    Sort(sortOrder);
```

`selected` is the index of the current item. The function returns `true` on the frame the selection changed.
The label identifies the button and is not drawn. Items can also be passed as a
`std::span<const std::string_view>`.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ControlSize` | `ControlSize` | `Regular` | |
| `Width` | `Size` | `Fit` | `Fit` makes the button as wide as its widest item |
| `Disabled` | `bool` | `false` | |

## Behaviour

- The button shows the current item and an accent-colored square with two chevrons: the sign that it opens a
  list of choices.
- The menu opens when the mouse button goes down. It opens **over** the button, with the current item exactly
  where the button showed it, so the pointer is already on it. The current item has a checkmark.
- Choosing the current item closes the menu without a change.

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Focus the button |
| Space, Enter, Up / Down arrow | Open the menu with the current item highlighted |
| Up / Down arrow (menu open) | Move the highlight |
| Enter, Space (menu open) | Choose |
| Escape | Close without a change; focus returns to the button |

## Guidance from the HIG

- Use a pop-up button for a flat list of mutually exclusive values. For commands use a
  [PullDownButton](PullDownButton.md); for two to five choices that should all be visible, a
  [SegmentedControl](SegmentedControl.md).
- Put a label in front of the button that says what is being chosen ("Sort by").
- Let the default be the most likely choice.
