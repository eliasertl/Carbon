# ProgressIndicator

Shows that a task is running and, when known, how far it has come.
HIG: [Progress indicators](https://developer.apple.com/design/human-interface-guidelines/progress-indicators)
Library: CarbonExtensions, `#include <Carbon/Extensions/ProgressIndicator.h>`

```cpp
Carbon::ProgressIndicator(download.Fraction);                                  // a bar, 0 to 1
Carbon::ProgressIndicator(0.0f, { .IsIndeterminate = true });                  // a bar that shows activity
Carbon::ProgressIndicator(0.0f, { .Kind = Carbon::ProgressKind::Spinner, .IsIndeterminate = true });
Carbon::ProgressIndicator(0.4f, { .Kind = Carbon::ProgressKind::Spinner });    // a ring that closes
```

`value` goes from 0 to 1 and is clamped. The indicator is not interactive.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `Kind` | `ProgressKind` | `Bar` | `Bar` or `Spinner` |
| `IsIndeterminate` | `bool` | `false` | The length of the task is unknown; `value` is ignored |
| `Width` | `Size` | 180 | Length of a bar. Spinners have a fixed size. |
| `ControlSize` | `ControlSize` | `Regular` | Bars are 4, 6 and 8 points thick; spinners 12, 16 and 32 points wide |
| `Tint` | `Color` | theme's `Accent` | The filled part. An indeterminate spinner uses the secondary label color. |

## Behaviour

- An indeterminate bar sends a segment across the track; an indeterminate spinner steps a bright spoke around
  eight spokes, as on macOS.
- Indeterminate indicators animate from the clock, so `IsAnimating()` stays `true` while one is shown and hosts
  that render on demand keep rendering. They keep animating with Reduce Motion: the motion is the information.
- A determinate indicator shows exactly the value it is given. To glide between values, animate the value you
  pass: `Animate(id, fraction)`.

## Guidance from the HIG

- Prefer a determinate indicator whenever the duration can be estimated; it tells people how long to wait.
- Switch from indeterminate to determinate as soon as the length becomes known.
- Use a spinner for short waits in a small space, a bar for longer tasks, and keep the indicator where the
  result will appear.
