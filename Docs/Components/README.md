# Components

Every component has a page with its purpose, the matching page of Apple's Human Interface Guidelines, an example,
its options and its keyboard behaviour.

## Core (`Carbon`)

| Component | Purpose |
| --- | --- |
| [Text](Text.md) | Displays text in one of the macOS text styles |
| [Icon](Icon.md) | Displays a Phosphor icon |
| [Button](Button.md) | Starts an action |
| [Toggle](Toggle.md) | A switch or checkbox bound to a `bool` |
| [Slider](Slider.md) | Picks a value from a range |
| [TextField](TextField.md) | Edits a single line of text |
| [Image](Image.md) | Displays a texture of the host |
| [Separator](Separator.md) | A thin line between items |
| [Tooltip](Tooltip.md) | Explains the control under the pointer |
| [Stacks and Spacer](Stack.md) | Lay items out vertically or horizontally |
| [ScrollView](ScrollView.md) | A clipped, scrollable area |

## Conventions shared by all components

- **Label and ID.** The first argument of an interactive component is its label, which is also its identity.
  `"Delete##row3"` shows "Delete" and keeps the ID unique; `"###save"` fixes the ID whatever the text before it.
  Inside loops, wrap items in `PushID(index)` / `PopID()`.
- **Options.** The last argument is an options struct, used with designated initializers. Every field is
  optional: `Button("Delete", { .Role = ButtonRole::Destructive, .CornerRadius = 12.0f })`. Fields must be named
  in the order they are declared, which is the order of the tables on each page.
- **Styling precedence.** An option given in the call beats the style stack
  (`PushStyleColor` / `PushStyleVar`), which beats the theme. See [Styling](../Styling.md).
- **Return value.** Components that change a value return `true` on the frame it changed; buttons on the frame
  they were activated.
- **Size.** `Width` (and `Height` where present) take a `Size`: `Size::Fit()`, a number of points, or
  `Size::Fill()`. See [Layout](../Layout.md#sizes).
- **Control sizes.** `ControlSize::Small`, `Regular` (default) and `Large` are 20, 24 and 30 points tall in the
  default theme.
- **Disabled.** `.Disabled = true`, or a `PushDisabled()` / `PopDisabled()` scope around several components,
  dims them and makes them ignore input and Tab.
- **After the call.** `IsItemHovered()`, `IsItemFocused()`, `IsItemActive()`, `GetItemRect()` and `Tooltip()`
  refer to the component submitted last.
