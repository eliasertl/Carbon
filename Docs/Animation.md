# Animation

Motion in Carbon is quick, subtle and interruptible. Values are driven by springs by default; timing curves are
available where a fixed duration fits better. Because the interface is rebuilt every frame, animating is a
matter of asking for a value each frame:

```cpp
const Carbon::ID id = Carbon::GetID("highlight");
const float y = Carbon::Animate(id, selectedRowY);     // glides to the row whenever the selection changes
```

## Animate

```cpp
float Animate(ID id, float target, const AnimationSpec& spec = AnimationSpec::Spring());
Vec2  Animate(ID id, Vec2 target,  const AnimationSpec& spec = AnimationSpec::Spring());
Rect  Animate(ID id, const Rect& target,  const AnimationSpec& spec = AnimationSpec::Spring());
Color Animate(ID id, const Color& target, const AnimationSpec& spec = AnimationSpec::Spring());
```

Call it every frame with the value you want. It returns the value to draw this frame.

- **The first call returns the target.** Nothing animates when a widget appears.
- **A new target is followed** according to the spec.
- **State is kept per ID** and dropped when a frame passes without a call, so a widget that reappears starts
  fresh. Use one ID per animated value, for example `GetID("hover")` and `GetID("press")` inside the widget's ID
  scope.
- **The value advances once per frame**, however often it is requested.
- `SetAnimationValue(id, value)` makes the value jump without animating.

## Springs

```cpp
AnimationSpec::Spring(float response = 0.3f, float dampingFraction = 1.0f)
```

Springs are parameterized like Apple's:

- `response` is roughly the time in seconds the spring takes to reach its target.
- `dampingFraction` is 1 for the fastest approach without overshoot (critically damped), less than 1 for a
  bounce, more than 1 for a slower, softer approach.

| Use | Spec |
| --- | --- |
| Most state changes: selection, expansion, scrolling | `Spring(0.25–0.35, 1.0)` |
| Small, fast feedback: hover, press | `Spring(0.15–0.2, 1.0)` |
| Where Apple bounces: a switch knob, a sheet arriving | `Spring(0.3–0.4, 0.75–0.85)` |

Carbon defaults to critically damped springs and uses bounce only where macOS does.

### Interruptible

When the target changes while a spring is moving, the spring keeps its current value **and velocity** and heads
for the new target. Nothing restarts and nothing jumps. This is what makes a sidebar highlight glide smoothly
when the user clicks quickly from item to item, and why springs are the default.

### Frame-rate independent

Carbon does not integrate the spring numerically. Each frame it evaluates the exact solution of the damped
harmonic oscillator for the frame's delta time, starting from the current value and velocity. Advancing by 16 ms
twice gives the same result as advancing by 32 ms once, so motion looks the same at 30, 60 and 144 Hz, and a
dropped frame does not disturb it.

`AdvanceSpring` and `IsSpringAtRest` (`Carbon/Animation/Spring.h`) expose the solver for your own simulations.

## Timing curves

```cpp
AnimationSpec::Ease(Easing::EaseInOut, 0.2f)    // curve and duration in seconds
AnimationSpec::Fade(0.15f)                      // a short ease-out for colors and opacity
AnimationSpec::None()                           // no animation
```

`Easing` is `Linear`, `EaseIn`, `EaseOut` or `EaseInOut`. When the target changes mid-way, the curve restarts
from the current value. `Ease(easing, t)` and `CubicBezier(x1, y1, x2, y2, t)` evaluate curves directly.

## Reduce motion

```cpp
Carbon::SetReduceMotion(true);
```

Some people are sensitive to movement. The host decides when to turn this on (Carbon cannot read the OS
setting); the Gallery example has a toggle for it. Each animation declares what it shows:

| Trait | What it is | With reduced motion |
| --- | --- | --- |
| `AnimationTrait::Motion` (default) | Something moves or changes size | Jumps to the target |
| `AnimationTrait::Appearance` | A color or opacity changes | A 150 ms cross-fade |

Colors are always treated as `Appearance`. For an opacity or a tint amount animated as a `float`, mark the spec:
`AnimationSpec::Spring(0.2f).AsAppearance()`, or use `AnimationSpec::Fade()`.

## Theme switches

`Carbon::SetTheme(theme)` animates every color and metric from its current value to the new theme's over 350 ms
(a 150 ms cross-fade with reduced motion). Widgets do nothing for this: they read their colors from the theme
each frame and get the blended values. Switching again mid-way continues from the current blend. Pass
`animated = false` to switch instantly. The first theme, set before any frame, always applies immediately.

## Rendering on demand

A host that renders only when something changes asks Carbon, after `EndFrame`, when the next frame is due:

```cpp
Carbon::EndFrame();
const float delay = Carbon::GetNextFrameDelay();      // seconds
if (delay == 0.0f)
    glfwPollEvents();                                 // something moves: render the next frame right away
else if (std::isinf(delay))
    glfwWaitEvents();                                 // nothing will change until there is input
else
    glfwWaitEventsTimeout(delay);                     // something changes by itself later: a caret, an indicator
```

- `IsAnimating()` is true when anything was still moving in the last frame: a spring or ease, a theme transition,
  a container fading in, or layout that needs one more frame to settle. `GetNextFrameDelay()` is 0 then.
- Otherwise `GetNextFrameDelay()` is the time until something changes without moving in between: the caret of a
  focused text field appears or disappears (every half second), a scroll indicator starts to fade (one second
  after the last scroll). It is infinity when nothing is due.

Set the delta time of the next frame to the time that really passed, as always: the caret and the indicator
count it. An animation that starts in the first frame after a sleep is not cut short by that: when nothing moved
in the frame before, the frame in which an animation starts counts for at most 1/30 s of it, so a click after ten
idle seconds still shows its animation from the beginning. A component that changes on a
schedule of its own calls `RequestFrameAfter(seconds)` in every frame in which it waits; one that animates from the
clock, like a spinner, calls `RequestAnimationFrame()`.

A focused text field used to keep `IsAnimating()` true for its caret, at 60 frames per second for a change twice
a second; a host that only looks at `IsAnimating()` would now see a caret that stops blinking.
