# Image

Displays a texture that belongs to the host: a rendered scene, a thumbnail, artwork.
HIG: [Image views](https://developer.apple.com/design/human-interface-guidelines/image-views)

![Images with rounded corners, a tint and a part of a texture, separated by separators](../Images/Components/Image.png)

```cpp
Carbon::TextureID id = Carbon::WebGPUGetTextureID(sceneView);                         // from the renderer backend
Carbon::Image(id, Carbon::Vec2(320.0f, 180.0f));
Carbon::Image(id, Carbon::Vec2(64.0f, 64.0f), { .CornerRadius = 12.0f });

Carbon::WebGPUImage(coverView, Carbon::Vec2(64.0f, 64.0f));                           // the same in one call

Carbon::Image(Carbon::MakeTextureID(coverView.Get()), Carbon::Vec2(64.0f, 64.0f));    // a raw handle, unregistered
```

The size is in points; the texture is stretched to it. A `TextureID` comes from the renderer backend that draws
the frame (`WebGPUGetTextureID` for a `wgpu::TextureView`; see [Renderer backends](../Backends.md)), and every
backend has an `Image` function that takes its own texture type directly (`WebGPUImage`). Like Dear ImGui's
`ImTextureID`, `MakeTextureID` also turns a raw native handle (a texture view's C handle, a Vulkan image view, an
OpenGL texture name) into an ID that can be drawn without registering it.

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `CornerRadius` | `float` | 0 | Rounds the corners with a squircle mask |
| `CornerSmoothing` | `float` | theme's `CornerSmoothing` | |
| `Tint` | `Color` | white | Multiplied with the texture's color |
| `UV` | `Rect` | `(0, 0, 1, 1)` | The part of the texture to show |
| `Zoomable` | `bool` | `false` | Two fingers pinch the image to zoom it and pan it while zoomed; a double tap zooms in and out ([Phones and tablets](../Mobile.md#gestures)). The mouse does not zoom |
| `MaxZoom` | `float` | 4 | The largest zoom of a zoomable image |
| `ID` | `string_view` | the texture | The identity that keeps the zoom; give one when a texture is shown twice |

## Notes

- Carbon keeps what the backend needs to draw the texture while it is drawn, and releases it once a frame passes
  without it. Keep the texture alive until then.
- Textures are sampled with linear filtering and treated as straight (non-premultiplied) alpha.
- Carbon does not load image files. Decode them with a library of your choice and upload them as textures.
- To show your own 3D viewport inside the interface, render it to a texture and pass the view here, or reserve
  the area with `AllocateItem` and draw into the same pass yourself, as `Examples/Minimal/WebGPUMinimal.cpp`
  does.

## Keyboard

Images are not interactive. `Tooltip` and `IsItemHovered()` work after them.
