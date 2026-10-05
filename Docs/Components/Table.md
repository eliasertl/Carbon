# Table

Rows of data in columns, with a header and a selection.
HIG: [Lists and tables](https://developer.apple.com/design/human-interface-guidelines/lists-and-tables)
Library: CarbonExtensions, `#include <Carbon/Extensions/Table.h>`

![A table of tasks with checkboxes, owners and estimates](../Images/Components/Table.png)

```cpp
const Carbon::TableColumn columns[] = {
    { .Title = "Done", .Width = 52.0f },
    { .Title = "Task" },                                                             // fills the rest
    { .Title = "Estimate", .Width = 80.0f, .Alignment = Carbon::TextAlignment::Trailing },
};

Carbon::BeginTable("tasks", columns, { .Height = 240.0f });
for (int i = 0; i < int(tasks.size()); i++)
{
    if (Carbon::TableRow(i, i == selected))
        selected = i;

    Carbon::BeginTableCell();                                                        // any content
    Carbon::Toggle("##done", &tasks[i].Done, { .Kind = Carbon::ToggleKind::Checkbox });
    Carbon::EndTableCell();
    Carbon::TableCell(tasks[i].Title);                                               // text
    Carbon::TableCell(tasks[i].Estimate, { .Secondary = true });
}
Carbon::EndTable();
```

| Function | Purpose |
| --- | --- |
| `BeginTable(id, columns, options)` / `EndTable()` | The table. At most 16 columns. |
| `TableRow(id, isSelected)` | Starts the next row; its cells follow. Returns `true` when the user picks the row. The ID is a number or a string, unique within the table. |
| `TableCell(text, options)` | The next cell, as text |
| `BeginTableCell()` / `EndTableCell()` | The next cell with content of your own, laid out like in an `HStack` |

A row ends where the next one starts. Its ID is pushed on the ID stack until then, so widgets in the cells of
different rows may share their labels (`"##done"` above). As with [List](List.md), the application owns the
selection.

## Columns

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Title` | `std::string_view` | none | Shown in the header |
| `Width` | `Size` | `Fill` | A number of points, or `Size::Fill(weight)` to share what the fixed columns leave |
| `Alignment` | `TextAlignment` | `Leading` | Of the title and of text cells. Align numbers `Trailing`. |

## Table options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Width` | `Size` | `Fill` | |
| `Height` | `Size` | `Fill` | A table scrolls, so inside a stack that fits its content give it a fixed height |
| `RowHeight` | `float` | 24 | |
| `ShowsHeader` | `bool` | `true` | |
| `ShowsAlternatingRows` | `bool` | `true` | Tints every other row |

## Cell options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Secondary` | `bool` | `false` | Secondary label color |
| `Icon` | `std::string_view` | none | Before the text |

## Behaviour

- The header stays in place while the rows scroll.
- Text that does not fit its cell is cut off with an ellipsis.
- A click on a widget inside a cell goes to the widget, not to the row.
- A row with fewer cells than columns leaves the rest empty; more cells than columns is reported as an error.
- Columns are not resizable or sortable by the user. To sort, reorder your data; the header is not interactive.

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Focus the table (one stop), then the widgets inside its cells |
| Up / Down arrow | Select the previous / next row |
| Home / End | Select the first / last row |

## Guidance from the HIG

- Give every column a short noun as its title, and put the column that identifies the row first.
- Alternating row backgrounds help in wide tables; in a narrow one they are noise.
