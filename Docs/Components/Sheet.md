# Sheet

A modal view for a self-contained task.
HIG: [Sheets](https://developer.apple.com/design/human-interface-guidelines/sheets)
Library: CarbonExtensions, `#include <Carbon/Extensions/Sheet.h>`

```cpp
if (Carbon::Button("Export..."))
    Carbon::OpenSheet("export");
if (Carbon::BeginSheet("export", { .Width = 400.0f }))
{
    Carbon::Text("Export", { .Style = Carbon::TextStyle::Title2, .Emphasized = true });
    Carbon::TextField("Name", &name, { .Width = Carbon::Size::Fill() });

    Carbon::BeginHStack({ .Width = Carbon::Size::Fill() });
    Carbon::Spacer();
    if (Carbon::Button("Cancel"))
        Carbon::CloseCurrentSheet();
    if (Carbon::Button("Export", { .Role = Carbon::ButtonRole::Prominent, .IsDefault = true }))
    {
        Export(name);
        Carbon::CloseCurrentSheet();
    }
    Carbon::EndHStack();
    Carbon::EndSheet();
}
```

`BeginSheet` returns `true` while the sheet is open; then add its content, laid out like in a `VStack`, and call
`EndSheet`.

| Function | Purpose |
| --- | --- |
| `OpenSheet(id)`, `CloseSheet(id)`, `IsSheetOpen(id)` | From outside the sheet, at the same ID scope as `BeginSheet` |
| `CloseCurrentSheet()` | From inside the sheet's content |

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Width` | `Size` | 420 | |
| `Height` | `Size` | `Fit` | |
| `Padding` | `EdgeInsets` | 20 | |
| `Spacing` | `float` | theme's `Spacing` | |
| `DismissOnEscape` | `bool` | `true` | Off: the sheet can only be closed through its own buttons |

## Behaviour

- The sheet is a card in the middle of the display above a scrim, as on macOS 11 and later. Everything beneath
  is dimmed and does not react; a click outside does nothing.
- It settles into place from slightly above and fades in (it only fades with Reduce Motion), and closes at
  once.
- Menus, pop-up buttons and popovers work inside a sheet; they stack above it.

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Cycle through the sheet's controls |
| Enter | Activate the sheet's `IsDefault` button |
| Escape | Close the sheet (unless `DismissOnEscape` is off); focus returns to where it was |

## Guidance from the HIG

- Use a sheet for a task that needs the user's full attention before work continues: export settings, a form,
  a confirmation with options. For a simple question use an [Alert](Alert.md).
- Always give a sheet a button that completes the task and one that cancels it, the completing one on the
  trailing side.
- Do not open a sheet from a sheet; keep the task short enough to fit one.
