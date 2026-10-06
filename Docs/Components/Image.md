# Image

Displays a texture that belongs to the host: a rendered scene, a thumbnail, artwork.
HIG: [Image views](https://developer.apple.com/design/human-interface-guidelines/image-views)

![Images with rounded corners, a tint and a part of a texture, separated by separators](../Images/Components/Image.png)

```cpp
Carbon::TextureID id = Carbon::WebGPUGetTextureID(sceneView);                         // from the renderer backend
Carbon::Image(id, Carbon::Vec2(320.0f, 180.0f));
Carbon::Image(id, Carbon::Vec2(64.0f, 64.0f), { .CornerRadius = 12.0f });

Carbon::WebGPUImage(coverView, Carbon::Vec2(64.0f, 64.0f));                           // the same in one call
```

The size is in points; the texture is stretched to it. A `TextureID` comes from the renderer backend that draws
the frame (`WebGPUGetTextureID` for a `wgpu::TextureView`; see [Renderer backends](../Backends.md)), and every
backend has an `Image` function that takes its own texture type directly (`WebGPUImage`).

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `CornerRadius` | `float` | 0 | Rounds the corners with a squircle mask |
| `CornerSmoothing` | `float` | theme's `CornerSmoothing` | |
| `Tint` | `Color` | white | Multiplied with the texture's color |
| `UV` | `Rect` | `(0, 0, 1, 1)` | The part of the texture to show |

## Notes

- Carbon keeps what the backend needs to draw the texture while it is drawn, and releases it once a frame passes
  without it. Keep the texture alive until then.
- Textures are sampled with linear filtering and treated as straight (non-premultiplied) alpha.
- Carbon does not load image files. Decode them with a library of your choice and upload them as textures.
- To show your own 3D viewport inside the interface, render it to a texture and pass the view here, or reserve
  the area with `AllocateItem` and draw into the same pass yourself, as `Examples/WebGPUMinimalIntegration` does.

## Keyboard

Images are not interactive. `Tooltip` and `IsItemHovered()` work after them.
