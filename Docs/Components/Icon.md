# Icon

Displays one icon from the embedded [Phosphor](https://phosphoricons.com) set.
HIG: [Icons](https://developer.apple.com/design/human-interface-guidelines/icons)

```cpp
Carbon::Icon(Carbon::Icons::Gear);
Carbon::Icon(Carbon::Icons::Heart, { .Size = 24.0f,
                                     .Color = Carbon::GetStyleColor(Carbon::StyleColor::Red),
                                     .Variant = Carbon::IconVariant::Fill });
```

The icon occupies a square of `Size` points. Every icon is a constant in `Carbon::Icons`, named after the
Phosphor icon in PascalCase (`address-book` is `Icons::AddressBook`). `Carbon::Icons::All` lists all 1530.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Size` | `float` | 16 | Edge length of the icon's square, in points |
| `Color` | `Color` | theme's `Label` | |
| `Variant` | `IconVariant` | `Regular` | `Regular`, `Bold` or `Fill` |

## Icons inside text

The constants are UTF-8 strings, so an icon can also be part of any label, where it follows the text's size,
color and weight:

```cpp
Carbon::Button("Add", { .Icon = Carbon::Icons::Plus });
Carbon::Text(std::string(Carbon::Icons::Warning) + "  Low disk space");
```

## Keyboard

Icons are not interactive. For a clickable icon use `Button("##id", { .Icon = ... })`, which is a Tab stop and
can carry a `Tooltip`.

## Guidance from the HIG

- Use an icon for a familiar action only where its meaning is clear; add a tooltip to icon-only buttons.
- Keep one weight per context; reserve the filled variant for selected or emphasized states.
