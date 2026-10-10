# List

A scrolling column of rows with a selection.
HIG: [Lists and tables](https://developer.apple.com/design/human-interface-guidelines/lists-and-tables)
Library: CarbonExtensions, `#include <Carbon/Extensions/List.h>`

![A list of files with their sizes and a selected row](../Images/Components/List.png)

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
| `AllowsReordering` | `bool` | `false` | The user can drag items to another place; see [Reordering](#reordering) |

## Item options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Icon` | `std::string_view` | none | Before the title |
| `Detail` | `std::string_view` | none | Secondary text at the trailing edge |
| `Disabled` | `bool` | `false` | Dimmed; cannot be picked and is skipped by the keyboard |

## Behaviour

- Rows span the list. The selected row has a rounded highlight: gray, or the selection color with on-accent
  text while the list has keyboard focus.
- A row under the pointer is tinted. A row is picked when the mouse button goes down, as in macOS lists, and
  clicking it gives the list focus. Tables, outline views and column views behave the same.
- Titles that do not fit are cut off with an ellipsis.
- Rows outside the visible area are not drawn. For very long lists, submit only the rows that are needed with
  `ClipListItems`; see [Long lists](#long-lists).

## Reordering

With `AllowsReordering`, the user drags an item to another place. The item stays where it is, faded, while a
preview follows the pointer; the other items slide apart on a spring to make room where it would go, and an
insertion line marks the place. Near the top and bottom edges the list scrolls. On release, `EndList` returns a
`ListMove`, and `ApplyListMove` applies it to your items; afterwards every item slides from where it was drawn
into its new place. Escape cancels.

![A song being dragged to another place in a playlist](../Images/Components/ListReordering.png)

```cpp
Carbon::BeginList("playlist", { .Height = 200.0f, .AllowsReordering = true });
for (const Song& song : songs)
{
    Carbon::PushID(song.Id);             // an ID that follows the item, not its position
    if (Carbon::ListItem(song.Title, song.Id == selected))
        selected = song.Id;
    Carbon::PopID();
}
Carbon::ApplyListMove(songs, Carbon::EndList());
```

| `ListMove` field | Meaning |
| --- | --- |
| `From` | The index of the item that moved, or -1 |
| `To` | The index it has after the move: where it ends up once it has been taken out and put back in |
| `IsMoved()` | True when an item moved; a drop next to its own place moves nothing |

`ApplyListMove(items, move)` rotates the items between `From` and `To` of any container with random-access
iterators, and does nothing for a move that did not happen. Give items IDs that follow them (`PushID` of a key
rather than of the index), so that they slide into their new places instead of trading contents. One item moves
at a time; disabled items cannot be dragged. Indices count every item of the list, including the ones that
`ClipListItems` left out, and the dragged item is kept among the submitted ones so that its preview stays. See
[Drag and drop](../DragAndDrop.md) for dragging between lists and other targets.

## Long lists

An item that is scrolled out of view costs little: it takes its space, but it is not hit-tested, its label is not
hashed or shaped and nothing is drawn. For tens of thousands of items and more, submit only the ones that are
needed. `ClipListItems(count, selectedItem)` declares how many items the list has and returns the range to submit:
the items in view and, after the arrow keys, Home or End moved the selection, the item they moved to.

```cpp
Carbon::BeginList("files", { .Height = 200.0f });
const Carbon::RowRange items = Carbon::ClipListItems(int(names.size()), selected);
for (int i = items.First; i < items.End; i++)
{
    if (Carbon::ListItem(names[i], i == selected))
        selected = i;
}
Carbon::EndList();
```

The list reserves the space of the other items, so it scrolls and reacts to the keyboard as if all of them were
submitted. Call it once, right after `BeginList`, and submit exactly the items of the range, in order. The items
that are left out count as enabled. [Table](Table.md#long-tables) has the same for its rows.

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
