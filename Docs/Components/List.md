# List

A scrolling column of rows with a selection.
HIG: [Lists and tables](https://developer.apple.com/design/human-interface-guidelines/lists-and-tables)
Library: CarbonExtensions, `#include <Carbon/Extensions/List.h>`

![A list and a table](../Images/Gallery-Lists-Light.png)

```cpp
Carbon::BeginList("files", { .Height = 200.0f });
for (int i = 0; i < int(files.size()); i++)
{
    Carbon::PushID(i);
    if (Carbon::ListItem(files[i].Name, i == selected, { .Icon = Carbon::Icons::FileText, .Detail = files[i].Size }))
        selected = i;
    Carbon::PopID();
}
Carbon::EndList();
```

The application owns the selection: pass each row whether it is selected, and react when `ListItem` returns
`true`. That makes single selection (`selected = i`), multiple selection (`isSelected[i] = !isSelected[i]`) and
no selection at all the application's choice. For rows with several columns see [Table](Table.md).

## List options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Width` | `Size` | `Fill` | |
| `Height` | `Size` | `Fill` | A list scrolls, so inside a stack that fits its content give it a fixed height |
| `RowHeight` | `float` | 24 | |
| `HasBorder` | `bool` | `true` | Draws the list on a bordered control background. Turn it off for a list that fills a pane. |

## Item options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Icon` | `std::string_view` | none | Before the title |
| `Detail` | `std::string_view` | none | Secondary text at the trailing edge |
| `Disabled` | `bool` | `false` | Dimmed; cannot be picked and is skipped by the keyboard |

## Behaviour

- Rows span the list. The selected row has a rounded highlight: gray, or the selection color with on-accent
  text while the list has keyboard focus.
- Clicking a row gives the list focus.
- Titles that do not fit are cut off with an ellipsis.
- All rows are submitted every frame; rows outside the visible area are not drawn. For very long lists, submit
  only the rows near the visible range and reserve the rest of the height with a `Spacer`.

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Focus the list; it is one stop, and shows the focus ring |
| Up / Down arrow | Select the previous / next row |
| Home / End | Select the first / last row |

A selection made by keyboard is scrolled into view. A key press takes effect on the following frame, through
the return value of the row it moves to.

## Guidance from the HIG

- Keep row text short so that it is not cut off, and sort rows in an order people expect.
- Use a list for one column of similar items; when items have attributes worth comparing, use a table.
