# Overlays

An overlay is a surface that floats above the interface: a popover, a menu, an alert, a sheet. Carbon has no
windows of its own, so overlays are part of the same frame as everything else. They draw in a layer above the
interface, and while one is open it can take the pointer and the keyboard away from what lies beneath.

Most applications use the ready-made components in CarbonExtensions:
[Popover](Components/Popover.md), [Menu](Components/Menu.md), [Alert](Components/Alert.md) and
[Sheet](Components/Sheet.md). This page describes the mechanism they are built on, for components of your own.
Include `Carbon/Overlay/Overlay.h` (it is part of `Carbon/Carbon.h` and `Carbon/Extension.h`).

## Opening and building

```cpp
const Carbon::ID id = Carbon::GetID("options");

if (Carbon::Button("Options"))
    Carbon::OpenOverlay(id);

if (Carbon::BeginOverlay(id, { .Anchor = Carbon::GetItemRect() }))
{
    Carbon::Text("Laid out like a VStack");
    if (Carbon::Button("Done"))
        Carbon::CloseCurrentOverlay();
    Carbon::EndOverlay();
}
```

- `OpenOverlay(id)` marks the overlay as open. Nothing is shown until `BeginOverlay` is called for it.
- `BeginOverlay` returns `false` while the overlay is closed; then skip the content and do **not** call
  `EndOverlay`. While it is open, call it every frame: an open overlay that is not submitted during a frame
  closes by itself. That is what makes a popover disappear together with the page it belongs to.
- The content is laid out top to bottom like in a `VStack`, and `id` is pushed on the ID stack.
- `CloseOverlay(id)` closes it from outside, `CloseCurrentOverlay()` from inside its content.
  `IsOverlayOpen(id)` and `IsAnyOverlayOpen()` tell the state.

The overlay does not have to be built near the control that opens it. It can be submitted anywhere in the frame;
it always draws above.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Anchor` | `Rect` | empty | The rectangle the overlay belongs to. A rectangle without size is a point. |
| `Placement` | `OverlayPlacement` | `Below` | `Below`, `Above`, `Trailing`, `Leading` of the anchor; `Center` of the display; `Top` of the display |
| `Alignment` | `Alignment` | `Leading` | Where the overlay sits along the anchor's edge |
| `Gap` | `float` | 4 | Distance between anchor and overlay (for `Top`: distance from the display's edge) |
| `IsModal` | `bool` | `false` | Blocks the interface beneath; outside clicks do nothing |
| `HasScrim` | `bool` | `false` | Dims everything beneath |
| `DismissOnOutsideClick` | `bool` | `true` | A click outside closes the overlay and is used up |
| `DismissOnEscape` | `bool` | `true` | Escape closes the overlay while it is the topmost one |
| `ShowsArrow` | `bool` | `false` | An arrow pointing at the anchor |
| `Padding` | `EdgeInsets` | 12 | Space around the content |
| `Spacing` | `float` | theme's `Spacing` | Distance between items |
| `Width`, `Height` | `Size` | `Fit` | `Fit` sizes the overlay to its content |
| `ContentAlignment` | `Alignment` | `Leading` | Where items sit horizontally |
| `CornerRadius` | `float` | theme's `OverlayCornerRadius` | |

## Placement

An anchored overlay sits on the requested side of its anchor. When that side does not have enough room and the
opposite side has more, the overlay flips over. In every case it is then moved to stay 8 points inside the
display.

The size used for placing is the one measured in the previous frame. A newly opened overlay has no size yet: it
is laid out once invisibly, and fades in from the next frame on. Closing is immediate.

`Center` and `Top` overlays settle into place from slightly above; with Reduce Motion they only fade.

## Stacking

Open overlays form a stack in the order they were opened.

- A later overlay draws above an earlier one, whatever the order they are submitted in. Up to eight overlays
  can be stacked; deeper ones share the top layer.
- Closing an overlay closes every overlay opened after it. A menu opened from a popover goes away with the
  popover.
- Tooltips draw above all overlays.

## Pointer and keyboard

An overlay that is modal or dismisses on outside clicks **holds** the pointer and the keyboard while it is the
topmost one:

- Nothing beneath is hovered or clicked. A click outside is used up: it closes the overlay (or does nothing, for
  a modal one) and does not reach the control under the pointer. A right or middle click outside dismisses too.
- Tab and Shift+Tab cycle through the overlay's own controls. Focus is taken away from whatever had it when the
  overlay appears and given back when the overlay closes.
- Enter goes to the default button inside the overlay, not to one beneath.
- Scroll views beneath do not scroll.
- Escape closes the topmost overlay, one per key press.

An overlay with `IsModal = false` and `DismissOnOutsideClick = false` is a panel that stays while the user works
elsewhere: it covers only its own surface, and its controls are part of the normal Tab order.

`IsRectHovered` returns `false` for a rectangle that an overlay covers, so custom components need no code of
their own to behave correctly under overlays.

## For component authors

- Derive the overlay's ID from your component's: `HashID("##menu", id)`.
- To open on a press rather than a release, use `ButtonBehaviorOptions::ActivateOnPress`.
- An overlay built inside another overlay's content (a submenu, a pop-up button in a sheet) is a separate entry
  of the stack with its own focus scope. It closes with its parent because it is no longer submitted.
- `IO::WantsMouse()` is true while an overlay holds the pointer, so the host does not act on clicks meant to
  dismiss it.
