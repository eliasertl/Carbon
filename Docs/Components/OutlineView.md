# OutlineView

Hierarchical data in rows, with disclosure triangles: a file browser, the outline of a document.
HIG: [Outline views](https://developer.apple.com/design/human-interface-guidelines/outline-views)
Library: CarbonExtensions, `#include <Carbon/Extensions/OutlineView.h>`

![An outline view of files with Name, Size and Kind columns](../Images/Components/OutlineView.png)

```cpp
void BuildNode(const FileNode& node)
{
    const Carbon::OutlineItem item = Carbon::BeginOutlineItem(node.Name, selected == &node,
        { .Icon = node.IsFolder() ? Carbon::Icons::Folder : Carbon::Icons::FileText,
          .HasChildren = node.IsFolder() });
    Carbon::OutlineCell(node.Size, { .Secondary = true });   // further columns, right after Begin
    if (item.Picked)
        selected = &node;
    if (item.Activated)
        Open(node);
    if (item.IsExpanded)
        for (const FileNode& child : node.Children)
            BuildNode(child);
    Carbon::EndOutlineItem();                              // always, after the children
}

const Carbon::TableColumn columns[] = { { .Title = "Name" }, { .Title = "Size", .Width = 80.0f } };
Carbon::BeginOutlineView("files", { .Height = 300.0f, .Columns = columns });
for (const FileNode& node : root)
    BuildNode(node);
Carbon::EndOutlineView();
```

| Function | Purpose |
| --- | --- |
| `BeginOutlineView(id, options)` / `EndOutlineView()` | The outline view. `EndOutlineView` returns the `OutlineMove` of an item the user dragged elsewhere; see [Moving items](#moving-items) |
| `BeginOutlineItem(label, isSelected, options)` | An item. Returns an `OutlineItem`: `IsExpanded` (add the children now), `Picked` (make it the selection), `Activated` (double-click or Enter: open it). |
| `EndOutlineItem()` | Ends the item, after its children. Needed for every item, leaves too. |
| `OutlineCell(text, options)` | The next column of the item, right after `BeginOutlineItem` |

Each item pushes its ID for its children, so labels only need to be unique among siblings. The application owns
the selection; Carbon remembers which items are expanded, also while they are not shown.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Width`, `Height` | `Size` | `Fill` | An outline view scrolls: inside a stack that fits its content, give it a fixed height |
| `RowHeight` | `float` | 24 | |
| `Columns` | `std::span<const TableColumn>` | none | Columns as in a [Table](Table.md). The first holds the hierarchy; the others are filled with `OutlineCell`. Without columns there is no header. |
| `ShowsAlternatingRows` | `bool` | `false` | Tints every other row |
| `HasBorder` | `bool` | `true` | Bordered control background. Turn it off for an outline view that fills a pane. |
| `AllowsReordering` | `bool` | `false` | The user can drag items that have a `Key` to move them |

## Item options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Icon` | `std::string_view` | none | Before the title |
| `IconTint` | `Color` | theme's `Accent` | |
| `HasChildren` | `bool` | `true` | Shows a disclosure triangle. Leaves set it to `false`. |
| `IsInitiallyExpanded` | `bool` | `false` | Expansion the first time the item appears; afterwards the user's choice is remembered |
| `Disabled` | `bool` | `false` | Cannot be picked; skipped by the keyboard |
| `Key` | `int64_t` | -1 | Identifies the item in an `OutlineMove`: an index or ID of yours, 0 or more. Items without a key cannot be moved or receive items |

## Behaviour

- Each level is indented by 16 points. The disclosure triangle turns as the item expands.
- A click on the triangle expands or collapses without selecting; Alt-click does it for everything inside.
- Titles that do not fit their column end with an ellipsis.
- Rows, selection highlight, hover tint and scrolling behave as in a [List](List.md).

## Moving items

With `AllowsReordering`, the user drags an item to another place in the hierarchy. Where it would go depends on
the part of the row under the pointer:

- the upper quarter of an item that can contain others, or the upper half of a leaf: **before** it, among its
  siblings, marked by an insertion line indented like the item;
- the lower quarter (half): **after** it. Below an expanded item, that means before its first child, inside it,
  as in Finder;
- the middle of an item that can contain others: **into** it, as its last child, marked by an outline around the
  item. A collapsed item that the drag rests on for 0.7 seconds expands, so that the item can go deeper;
- below the last item: the end of the top level.

An item cannot go into itself or anything inside it; over those rows no indicator shows and a drop does nothing.

![An item being dragged into a folder of an outline view](../Images/Components/OutlineViewReordering.png)

Carbon does not own your tree. `EndOutlineView` reports the move by the keys you gave the items, and you apply it:

```cpp
Carbon::BeginOutlineView("files", { .Height = 300.0f, .AllowsReordering = true });
BuildItems(root);                        // BeginOutlineItem(name, ..., { .Key = node.Id }) for each node
const Carbon::OutlineMove move = Carbon::EndOutlineView();
if (move.IsMoved())
    tree.Move(move.Item, move.Target, move.Position);
```

| `OutlineMove` field | Meaning |
| --- | --- |
| `Item` | The key of the item that moved, or -1 |
| `Target` | The key of the item it goes before, after or into; -1 for the root |
| `Position` | `OutlineDropPosition::Before`, `After` or `Into` (as the last child) |

## Long outlines

An item that is scrolled out of view costs little: it takes its space and keeps its place in the hierarchy, but
it is not hit-tested, its cells are not shaped and nothing is drawn. An outline has no counterpart to
[`ClipTableRows`](Table.md#long-tables), because which items exist depends on which are expanded: your code still
walks every expanded item. [Optimizations](../Optimizations.md#rows) has the numbers for 100,000 of them.

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Focus the outline view; it is one stop |
| Up / Down arrow, Home / End | Select the previous / next / first / last visible item |
| Right arrow | Expand the selected item; when it is expanded, select its first child |
| Left arrow | Collapse the selected item; when it is collapsed or a leaf, select its parent |
| Alt + Right / Left | Expand / collapse the item and everything inside it |
| Enter | Activate the selected item |

## Not supported

Sorting by column, resizing columns and editing cells in place. Sort your data before submitting it. Moving
several items at once.

## Guidance from the HIG

- Use an outline view for hierarchical data only; for flat data use a [List](List.md) or [Table](Table.md).
- Show the hierarchy in the first column and attributes in the others, with short noun titles.
- Let people search a long outline: put a [SearchField](SearchField.md) nearby.
