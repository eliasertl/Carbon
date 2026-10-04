# VStack, HStack and Spacer

Lay items out in a column or a row.
HIG: [Layout](https://developer.apple.com/design/human-interface-guidelines/layout)

```cpp
Carbon::BeginVStack({ .Spacing = 12.0f, .Padding = 20.0f });
    Carbon::Text("Settings", { .Style = Carbon::TextStyle::LargeTitle });
    Carbon::Toggle("Dark Mode", &darkMode);
    Carbon::BeginHStack({ .Spacing = 8.0f, .Width = Carbon::Size::Fill() });
        Carbon::Spacer();
        Carbon::Button("Cancel");
        Carbon::Button("Save", { .Role = Carbon::ButtonRole::Prominent });
    Carbon::EndHStack();
Carbon::EndVStack();
```

Every `Begin` needs its `End`; forgetting one is reported at the end of the frame. The full model (sizes, free
space, the one frame of latency, identity) is explained in [Layout](../Layout.md).

## VStackOptions and HStackOptions

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Spacing` | `float` | theme's `Spacing` (8) | Distance between items |
| `Padding` | `EdgeInsets` | 0 | `20.0f`, `{horizontal, vertical}` or `{left, top, right, bottom}` |
| `Alignment` | `Alignment` (VStack), `VerticalAlignment` (HStack) | `Leading`, `Center` | Where items sit across the axis |
| `Justify` | `VerticalAlignment` (VStack), `Alignment` (HStack) | `Top`, `Leading` | Where the content sits along the axis when there is room and no spacer |
| `Width`, `Height` | `Size` | `Fit` | |
| `Background` | `Color` | none | A squircle behind the stack: a grouped box |
| `CornerRadius` | `float` | theme's `GroupCornerRadius` (10) | Corner radius of the background |
| `ID` | `std::string_view` | call site | A stable identity; see [Layout](../Layout.md#identity) |

A grouped box, as in System Settings:

```cpp
Carbon::BeginVStack({ .Padding = 16.0f,
                      .Width = Carbon::Size::Fill(),
                      .Background = Carbon::GetStyleColor(Carbon::StyleColor::SecondaryBackground) });
// rows ...
Carbon::EndVStack();
```

## Spacer

```cpp
Carbon::Spacer();                          // takes the free space of the stack
Carbon::Spacer({ .Length = 24.0f });       // a fixed gap
```

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Length` | `float` | none | A fixed length; the spacer then does not stretch |
| `MinLength` | `float` | 0 | The least a flexible spacer takes |
| `Weight` | `float` | 1 | Share of the free space relative to other spacers and `Fill` items |

A spacer only has free space to take when its stack's size along the axis is fixed or `Fill`.

## Keyboard

Stacks are not interactive. Tab moves through the controls inside them in the order they are submitted, which is
reading order.
