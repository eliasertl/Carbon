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
| `Wraps` | `HStack` only: items that do not fit move to a new line | `false` |

A vertical stack aligns its items with `Alignment::Leading`, `Center` or `Trailing`; a horizontal stack with
`VerticalAlignment::Top`, `Center` or `Bottom`. `Justify` uses the other of the two enums.

A horizontal stack with `Wraps` set and a width that is not `Fit` starts a new line, `Spacing` below, when the
next item does not fit; see [Stacks](Components/Stack.md#wrapping).

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
- Page Up and Page Down scroll the view under the pointer (or the outermost one), and so do Home and End when no
  control has focus; see [Keyboard navigation](KeyboardNavigation.md).
- The overlay scroll indicator appears while scrolling and when the view first appears, then fades out, as on
  macOS. It widens under the pointer and can be dragged. `ShowsIndicator = false` hides it.
- During a [drag](DragAndDrop.md), holding the pointer near an edge of the view scrolls it.
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
- A stack that appears for the first time is drawn in that frame, at its final size, whenever its first layout
  did not need its own measurements (see below). Otherwise it is hidden for exactly one frame and then fades in
  over about a tenth of a second, by which time its measurements exist.
- When content changes size, dependent positions follow one frame later. `Carbon::IsAnimating()` returns true
  until the layout has settled, so a host that renders on demand knows to render another frame.

### The first frame of a new stack

A new stack has no measurements yet. Carbon draws it anyway and checks, item by item, whether any placement used
a measurement the stack does not have. Its size is never the problem: a stack that fits its content knows that
size when it ends, and its background is drawn with it. Only these placements can be wrong in the first frame:

| In a new stack | Wrong when |
| --- | --- |
| Items centered or trailing-aligned across an axis that fits the content | A larger item comes after a smaller one. The stack grows with each item, so if the largest comes first, or all are the same size, it is right. |
| Items that `Fill` across an axis that fits the content (a `Separator`) | A larger item comes after it |
| `Spacer` or `Fill` along an axis of given length (`Fixed` or `Fill`) | Always: the free space is measured |
| `Justify` other than leading along an axis of given length | Always |
| A new fitting stack placed off-leading across its parent's axis (centered in an `HStack`, for instance) | Always: it is aligned by its own size, which is known only at its end |
| A new grid or grid row, a new stack in a grid cell; an overlay | Always: column widths and overlay sizes are measured |

When nothing was wrong the stack stays as drawn, with no fade. When something was, everything it drew is made
transparent at its end, and it fades in from the next frame as before; a new stack inside one that is drawn is
judged on its own, so only the part that would be wrong waits. A stack whose cross axis has a fixed size never
depends on its own measurement there. Frames of a settled layout skip the check.

## Identity

A stack remembers its measurements between frames, so it needs an identity. By default that is the place in your
source where `BeginVStack` or `BeginHStack` is called (plus the enclosing container and the ID stack). This is
stable when stacks around it appear and disappear. Grids and grid rows work the same way.

**The rule: one line of source begins one stack per container and ID scope.** Two cases break it, and both look
innocent:

- a **loop** that begins a stack in each iteration, and
- a **helper function** that begins a stack and is called more than once in the same container: every call
  lands on the same line inside the helper.

Carbon still tells such stacks apart, by their order, but then their measurements belong to a position rather
than to an item: when an item before them disappears, the next one takes over its measurements for a frame and
jumps. So Carbon reports the line once per run, through the host's log callback, as a warning from `Layout`:

```
Settings.cpp:42: BeginHStack was called more than once from this line inside the same container during one
frame, from a loop or from a function called several times. [...] Give each its own identity: put
PushID(key) / PopID() around each call, or pass a unique .ID in the options of BeginHStack.
```

The fix is to give each stack an identity of its own: wrap each iteration or call in `PushID(key)` / `PopID()`,
or pass `.ID` (`GridRowOptions` has one too). The key should belong to the item, not to its position, so the
measurements travel with the item:

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

In a helper function, push the ID only around the `Begin` call. Stacks take their identity when they begin, so
the stack gets its own, and the widgets inside keep the IDs they would have without the helper:

```cpp
void BeginRow(std::string_view label)
{
    Carbon::PushID(label);
    Carbon::BeginHStack({ .Width = Carbon::Size::Fill() });
    Carbon::PopID();
    Carbon::Text(label, { .Secondary = true });
}
```

Stacks do not push onto the ID stack, so wrapping widgets in more stacks never changes the widgets' IDs or loses
their state. Scroll views are different: they take an explicit ID and push it.

## Adapting to the device

Carbon adapts its own components to touchscreens and narrow displays ([Phones and tablets](Mobile.md)). Layouts of
your own can do the same with two queries, which change only at the start of a frame:

- `Carbon::IsCompactWidth()` (or `GetSizeClass()`): the display is narrower than 600 points, as an iPhone in
  portrait. Stack what sits side by side, show fewer columns, move secondary content behind a button.
- `Carbon::IsTouchMode()`: the user works with a finger. Leave room around small controls, avoid what needs hover
  or precise dragging.

```cpp
const bool isCompact = Carbon::IsCompactWidth();
if (isCompact)
    Carbon::BeginVStack({ .Spacing = 16.0f, .Width = Carbon::Size::Fill() });
else
    Carbon::BeginHStack({ .Spacing = 24.0f, .Alignment = Carbon::VerticalAlignment::Top, .Width = Carbon::Size::Fill() });
    Preview();
    Settings();
if (isCompact)
    Carbon::EndVStack();
else
    Carbon::EndHStack();
```

Both are derived automatically and the host can force either one (`IO::SetSizeClassOverride`,
`IO::SetTouchModeOverride`), for example regular width on a narrow kiosk display. Layouts reflow by themselves when
the display is resized or rotated: `Fill` sizes take their share of the new size in the next frame, and nothing is
stretched.

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
[Custom components](CustomComponents.md) covers the rest of the extension API.
