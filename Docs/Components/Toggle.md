# Toggle

A switch or checkbox bound to a `bool`.
HIG: [Toggles](https://developer.apple.com/design/human-interface-guidelines/toggles)

```cpp
Carbon::Toggle("Wi-Fi", &wifi, { .Width = Carbon::Size::Fill() });                 // a settings row
Carbon::Toggle("Show hidden files", &showHidden, { .Kind = Carbon::ToggleKind::Checkbox });

if (Carbon::Toggle("Dark Mode", &dark))
    Carbon::SetTheme(dark ? Carbon::Theme::Dark() : Carbon::Theme::Light());
```

`Toggle` returns `true` on the frame the value changed. The whole row is clickable, label included.

## Kinds

| Kind | Layout | Use (HIG) |
| --- | --- | --- |
| `Switch` (default) | Label, then the switch. With a fixed or `Fill` width the switch moves to the trailing edge, like a row in System Settings. | Settings that deserve visual weight, or that control a group of other settings |
| `Checkbox` | Box, then the label | Lists and hierarchies of options; single on/off settings |

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Kind` | `ToggleKind` | `Switch` | |
| `ControlSize` | `ControlSize` | `Regular` | Switch: 30 × 18, 38 × 22, 46 × 26 points. Checkbox: 12, 14, 16 points. |
| `Width` | `Size` | `Fit` | |
| `Disabled` | `bool` | `false` | |
| `IsMixed` | `bool` | `false` | Checkbox only: shows a dash, for a parent whose children differ |
| `Tint` | `Color` | theme's `Accent` | Color of the "on" state |

## States

The switch knob glides with a slight bounce and the track cross-fades to the accent color; the checkmark fades
in. With reduced motion the knob jumps and colors cross-fade. The track and the box darken slightly under the
pointer and further while pressed.

A mixed checkbox for a group:

```cpp
bool all = a && b;
if (Carbon::Toggle("Select all", &all, { .Kind = Carbon::ToggleKind::Checkbox, .IsMixed = a != b }))
    a = b = all;
```

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Focus the toggle (the ring is drawn around the switch or box) |
| Space, Enter | Flip the value |

## Guidance from the HIG

- Use checkboxes, aligned and indented, to show a hierarchy of settings.
- Do not replace a checkbox with a switch just for looks; a switch carries more visual weight.
