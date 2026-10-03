# Integrating Carbon into a host application

Carbon never creates windows, devices or OS hooks. Your application (the *host*) owns all of that, forwards input
to Carbon and gives it a place to draw. This guide covers the host's side: creating a context, forwarding input,
driving frames and handling DPI.

> Carbon is under construction. Rendering through Dawn (`Carbon::Render`) arrives with milestone 2 and will be
> documented here; until then a context builds draw data that you can inspect with `Carbon::GetDrawData()`. The
> `Wants…` flags stay false until the widgets of milestone 4 exist.

## Creating a context

```cpp
#include <Carbon/Carbon.h>

Carbon::ContextDescription description;
description.Device = device;                       // wgpu::Device; leave null for a headless context
description.ColorFormat = surfaceFormat;           // format of the pass Carbon will render into
description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message) {
    std::println("[{}] {}: {}", Carbon::ToString(level), source, message);
};
description.Callbacks.GetClipboardText = [] { return ReadClipboard(); };          // returns std::string (UTF-8)
description.Callbacks.SetClipboardText = [](std::string_view text) { WriteClipboard(text); };
description.Callbacks.SetCursor = [](Carbon::Cursor cursor) { ShowCursor(cursor); };

Carbon::Context* context = Carbon::CreateContext(description);
// ...
Carbon::DestroyContext(context);
```

- Carbon has **one current context**. The first context you create becomes current; use
  `Carbon::SetCurrentContext` to switch between several.
- Every callback is optional. Without `Log`, messages are dropped; Carbon never prints on its own. Without the
  clipboard callbacks, copy and paste do nothing.
- If the pass you render into has a depth-stencil attachment or is multisampled, set `DepthStencilFormat` and
  `SampleCount` so Carbon's pipeline matches it.

## The frame loop

```cpp
Carbon::IO& io = Carbon::GetIO();
io.SetDisplaySize(widthInPoints, heightInPoints);
io.SetContentScale(contentScale);
io.SetDeltaTime(secondsSinceLastFrame);
// ... forward input events (see below) ...

Carbon::NewFrame();
// ... build the UI ...
Carbon::EndFrame();
```

Set the display size, content scale and delta time **before** `NewFrame`, every frame. `NewFrame` applies the
queued input; `EndFrame` produces the frame's draw data. Calling them out of order, or leaving a `PushID`,
`PushClipRect` or similar unbalanced, is reported through the log and the assert callback.

## Forwarding input

Call these from your window's event handlers; Carbon queues the events and applies them in the next `NewFrame`.

| Event | Call | Notes |
| --- | --- | --- |
| Mouse moved | `io.AddMousePosEvent(x, y)` | Points, relative to the top-left of Carbon's area |
| Mouse left the window | `io.AddMouseLeaveEvent()` | Ends hover |
| Mouse button | `io.AddMouseButtonEvent(button, down)` | `Left`, `Right`, `Middle`, `Back`, `Forward` |
| Scroll | `io.AddMouseWheelEvent(x, y)` | One unit per wheel notch; fractions for touchpads |
| Key | `io.AddKeyEvent(key, down)` | Map your key codes to `Carbon::Key`; include modifier keys |
| Text | `io.AddInputCharactersUTF8(text)` or `io.AddInputCharacter(codepoint)` | From the OS's character events, not from key codes |
| Window focus | `io.AddFocusEvent(focused)` | Losing focus releases all held keys and buttons |

Details worth knowing:

- **Modifiers are keys.** Send `LeftCtrl`, `RightShift` and so on as ordinary key events; Carbon derives the
  modifier state from them.
- **Key repeat is Carbon's job.** Repeated "down" events for a key that is already down are ignored; Carbon
  repeats held keys itself (0.4 s delay, then every 50 ms) so behaviour is identical on every platform.
- **Fast input is never lost.** If a press and a release of the same button arrive within one frame, Carbon
  applies them on two consecutive frames. The same holds for a key, and typed characters keep their order
  relative to editing keys such as Backspace.
- **Shortcuts** such as copy and paste use Ctrl. A host that prefers the Super/Command key calls
  `io.SetShortcutModifier(Carbon::KeyModifiers::Super)`.
- **Sharing input with your own content.** After `NewFrame`, `io.WantsMouse()`, `io.WantsKeyboard()` and
  `io.WantsTextInput()` tell you whether Carbon is using the mouse or keyboard, so your application can ignore
  those events for its own viewport.

## Points, pixels and DPI

All of Carbon's coordinates and sizes are **logical points**. The content scale is the number of physical pixels
per point: 1.0 on a standard display, 1.5 at 150 % scaling, 2.0 on a typical HiDPI display.

- Pass the display size in points: `framebufferSizeInPixels / contentScale`.
- Pass mouse positions in points too. Some window systems report them in pixels (divide by the content scale),
  others already in points.
- When the window moves to a display with a different scale, just pass the new value to `SetContentScale`; Carbon
  re-rasterizes text for it.

With GLFW, for example:

```cpp
float xScale, yScale;
glfwGetWindowContentScale(window, &xScale, &yScale);
int framebufferWidth, framebufferHeight;
glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

io.SetContentScale(xScale);
io.SetDisplaySize(float(framebufferWidth) / xScale, float(framebufferHeight) / xScale);
```

`Carbon::ContentScale` offers the conversions (`ToPixels`, `ToPoints`) and `Snap`, which moves a coordinate to
the nearest pixel boundary so edges stay crisp.

## Logging and asserts

- `Callbacks.Log` receives every message with a level (`Trace` … `Fatal`) and a source such as `"Core"` or
  `"Assert"`.
- API misuse (unbalanced stacks, calls outside a frame, invalid values) is checked in every build type. Carbon
  logs the failure at `Fatal` level and then recovers. In debug builds it also breaks into the debugger, unless
  you set `Callbacks.AssertFailed`, in which case your callback decides what happens.
