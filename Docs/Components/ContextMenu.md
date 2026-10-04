# ContextMenu

A menu of commands for the item under the pointer, opened with a right click.
HIG: [Context menus](https://developer.apple.com/design/human-interface-guidelines/context-menus)
Library: CarbonExtensions, `#include <Carbon/Extensions/Menu.h>`

```cpp
Carbon::Text("report.pdf");
if (Carbon::BeginContextMenu("file"))
{
    if (Carbon::MenuItem("Open"))
        Open();
    if (Carbon::MenuItem("Rename"))
        Rename();
    Carbon::MenuSeparator();
    if (Carbon::MenuItem("Move to Trash", { .IsDestructive = true }))
        Trash();
    Carbon::EndContextMenu();
}
```

`BeginContextMenu` belongs to the item submitted just before it: a right click on that item opens the menu at
the pointer. It returns `true` while the menu is open; then add items exactly as in a [Menu](Menu.md) and call
`EndContextMenu`.

## A larger target

The "item submitted last" is whatever widget came before the call. To give a whole area a context menu, tell
Carbon that the area is the last item:

```cpp
Carbon::BeginHStack({ .ID = "row" });
// ... the row's content ...
Carbon::EndHStack();

const Carbon::Rect area = Carbon::GetLastItemRect();
Carbon::Interaction hover;
hover.Hovered = Carbon::IsRectHovered(area);
Carbon::SetLastItem(Carbon::GetID("row"), area, hover);
if (Carbon::BeginContextMenu("row menu"))
    // ...
```

Inside loops, wrap each item and its context menu in `PushID(index)` / `PopID()` so that every item has its own
menu.

## Behaviour and keyboard

Everything described for [Menu](Menu.md#behaviour) applies: the highlight, submenus, the arrow keys, Enter and
Escape. A right click somewhere else closes the menu.

## Guidance from the HIG

- Offer only the commands that apply to the clicked item, the most likely ones first, and keep the list short.
- Every command of a context menu must also be available elsewhere: people may never discover the menu.
- Do not show keyboard shortcuts in context menus.
