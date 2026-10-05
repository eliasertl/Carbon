# Layout

Carbon lays the interface out with stacks. Inside a stack, each item goes where the layout cursor is and the cursor
advances along the stack's axis, so there is no `SameLine()` and no manual positioning. Everything is in points.

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

## Stacks

`BeginVStack` / `EndVStack` lay items out top to bottom; `BeginHStack` / `EndHStack` leading to trailing. Stacks
nest freely, and the area the host gives Carbon behaves like a vertical stack that fills the display.

| Option | Meaning | Default |
| --- | --- | --- |
| `Spacing` | Distance between items | The theme's `Spacing` (8) |
| `Padding` | Space between the stack's edges and its items; `20.0f`, `{h, v}` or `{l, t, r, b}` | 0 |
| `Alignment` | Where items sit *across* the axis | `Leading` (VStack), `Center` (HStack) |
| `Justify` | Where the content sits *along* the axis when there is room and no spacer | `Top` / `Leading` |
| `Width`, `Height` | Size of the stack; see below | `Fit` |
| `Background` | Draws a squircle behind the stack (a grouped box) | none |
| `CornerRadius` | Corner radius of the background | The theme's `GroupCornerRadius` |
| `ID` | A stable identity; rarely needed, see [Identity](#identity) | call site |

A vertical stack aligns its items with `Alignment::Leading`, `Center` or `Trailing`; a horizontal stack with
`VerticalAlignment::Top`, `Center` or `Bottom`. `Justify` uses the other of the two enums.

## Sizes

A `Size` describes one axis of an item or a stack:

| Size | Meaning |
| --- | --- |
| `Size::Fit()` | As large as the content. The default. |
| `Size::Fixed(n)` or just `n` | Exactly `n` points: `.Width = 240.0f`. |
| `Size::Fill(weight = 1)` | **Along** the parent's axis: a share of the parent's free space, proportional to the weight. **Across** it: the parent's full extent. |

Free space is what remains of the parent after its fixed and fitting items and its spacing. It is divided among
all `Fill` items and spacers by weight:

```cpp
Carbon::BeginHStack({ .Spacing = 10.0f, .Width = 320.0f });
    Sidebar({ .Width = Carbon::Size::Fill() });        // 80 points
    Content({ .Width = Carbon::Size::Fill(2.0f) });    // 160 points
    Inspector({ .Width = 60.0f });                     // 60 points
Carbon::EndHStack();
```

A stack that fits its content has no free space along its axis: `Fill` items and spacers inside it collapse to
their minimum. Give the stack a fixed or fill size along that axis when you want them to stretch.

## Spacer

`Carbon::Spacer()` takes up the free space of the current stack, pushing what follows to the far end. Several
spacers share the space by `Weight`; `MinLength` is the least a spacer takes; `Length` turns it into a fixed gap.

```cpp
Carbon::BeginHStack({ .Width = Carbon::Size::Fill() });
    Carbon::Text("Title");
    Carbon::Spacer();                        // pushes the button to the trailing edge
    Carbon::Button("Done");
Carbon::EndHStack();

Carbon::Spacer({ .Length = 24.0f });         // a fixed gap between sections
```

## Scroll views

```cpp
Carbon::BeginScrollView("log", { .Height = 200.0f });
    for (const std::string& line : lines)
        Carbon::Text(line);
Carbon::EndScrollView();
```

A scroll view clips its content and lays it out like a stack along the scrolling axis (`Axis::Vertical` by
default, `Axis::Horizontal` for a strip). Its `Width` and `Height` are the size of the visible area and default
to `Fill`; inside a stack that fits its content, give the scrolling axis a fixed size.

- The mouse wheel scrolls the innermost scroll view under the pointer. The view glides to its new offset; with
  reduced motion it jumps.
- The overlay scroll indicator appears while scrolling and when the view first appears, then fades out, as on
  macOS. `ShowsIndicator = false` hides it.
- The offset is kept per ID and survives while the view is not shown. `GetScrollOffset` and `SetScrollOffset`
  read and change it.
- Rows that are scrolled out of view cost no drawing: shapes and text outside the clip rectangle are dropped
  before they reach the GPU.
- `Padding` scrolls with the content.

## Grids

Stacks line items up along one axis. When items must also line up *across* rows, as the labels and controls of a
form do, use a grid: rows of cells whose columns are as wide as their widest cell.

```cpp
static constexpr Carbon::Alignment FormColumns[] = { Carbon::Alignment::Trailing, Carbon::Alignment::Leading };

Carbon::BeginGrid({ .HorizontalSpacing = 8.0f, .ColumnAlignments = FormColumns });
    Carbon::BeginGridRow();
        Carbon::Text("Name:");
        Carbon::TextField("Name", &name);
    Carbon::EndGridRow();
    Carbon::BeginGridRow();
        Carbon::Text("Volume:");
        Carbon::Slider("Volume", &volume, 0.0f, 1.0f);
    Carbon::EndGridRow();
Carbon::EndGrid();
```

- Every item in a row is one cell, whether it is a widget or a stack. Cells fill the columns from the leading
  edge on; `SetNextGridCell({ .ColumnSpan = 2 })` makes the next cell cover two columns.
- Cells are aligned inside their column by the grid's `Alignment`, overridden per column by `ColumnAlignments`
  and per cell by `SetNextGridCell`. Vertically, by the grid's `VerticalAlignment`, the row's `Alignment` and the
  cell's.
- `Width = Size::Fill()` in a cell takes the column's width. The column is still as wide as the widest content,
  so a column of such cells gives equal-width items.
- Items placed directly in the grid, outside a row, take the grid's width: a `Separator()` between groups of rows.
- Grids follow the identity rules of stacks and do not push onto the ID stack either.

[Grid](Components/Grid.md) lists every option.

## One frame of latency

Carbon runs your interface code once per frame and places each item immediately. Whatever depends on a size that
is not known yet at that moment uses the measurement from the previous frame:

- the size of a stack that fits its content, as seen from *inside* the stack (centering, trailing alignment);
- free space for spacers and `Fill` items;
- the width of a grid's columns;
- `Justify`.

This is invisible in practice:

- Leading and top alignment never need measurements, and a fitting stack reports its true size to its parent
  within the same frame.
- A stack that appears for the first time is hidden for exactly one frame and then fades in over about a tenth
  of a second, by which time its measurements exist.
- When content changes size, dependent positions follow one frame later. `Carbon::IsAnimating()` returns true
  until the layout has settled, so a host that renders on demand knows to render another frame.

## Identity

A stack remembers its measurements between frames, so it needs an identity. By default that is the place in your
source where `BeginVStack` or `BeginHStack` is called (plus the enclosing container and the ID stack). This is
stable when stacks around it appear and disappear, and several stacks begun from the same line in a loop are
told apart by their order.

Only one case needs help: a loop whose items change *order* or are removed from the middle. Wrap each iteration
in `PushID(item.Key)` / `PopID()` or pass `.ID`, and the measurements travel with the item:

```cpp
for (const Row& row : rows)
{
    Carbon::PushID(row.Key);
    Carbon::BeginHStack();
    // ...
    Carbon::EndHStack();
    Carbon::PopID();
}
```

Stacks do not push onto the ID stack, so wrapping widgets in more stacks never changes the widgets' IDs or loses
their state. Scroll views are different: they take an explicit ID and push it.

## The cursor as an escape hatch

`GetCursorPos()` returns where the next item will go. `SetCursorPos(position)` places the next item's top-left
corner at an absolute position instead; the flow continues after it. `GetContentRect()` is the content area of
the current container and `GetLastItemRect()` the rectangle of the item placed last.

## For component authors

A widget reserves its place with `AllocateItem`:

```cpp
Carbon::Rect rect = Carbon::AllocateItem(Carbon::Vec2(width, height), { .Width = options.Width });
```

It passes the size its content needs and lets the caller override either axis with a fixed or fill size. Item
origins are snapped to whole pixels, so shapes and text stay crisp at fractional content scales.
[Custom components](CustomComponents.md) covers the rest of the extension API (milestone 5).
