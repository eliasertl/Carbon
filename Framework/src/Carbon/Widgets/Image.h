#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"

namespace Carbon
{
    /// Options of Image. All fields are optional.
    struct ImageOptions
    {
        /// Rounds the image's corners with a squircle mask.
        float CornerRadius = 0.0f;
        /// Corner smoothing of the mask; the theme's CornerSmoothing when not set.
        std::optional<float> CornerSmoothing = {};
        /// Multiplied with the texture's color; white leaves it unchanged.
        Color Tint = Color::White();
        /// The part of the texture to show, in 0..1 coordinates.
        Rect UV = Rect(0.0f, 0.0f, 1.0f, 1.0f);
        /// Two fingers pinch the image to zoom it, up to MaxZoom, and pan it while it is zoomed in; a double tap
        /// zooms in and out (ZoomBehavior). The mouse does not zoom.
        bool Zoomable = false;
        float MaxZoom = 4.0f;
        /// The identity that keeps a zoomable image's zoom while it is shown. Derived from the texture when
        /// empty; give one when the same texture is shown twice.
        std::string_view ID = {};
    };

    /// Displays a texture at `size` points. Get the TextureID of one of your textures from the renderer
    /// backend's GetTextureID function (WebGPUGetTextureID, VulkanGetTextureID, ...), or from its raw native handle
    /// with MakeTextureID, or use the backend's Image function that takes the texture directly (WebGPUImage, ...).
    void Image(TextureID texture, Vec2 size, const ImageOptions& options = {});
} // namespace Carbon
