#pragma once

#include <optional>

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
    };

    /// Displays a texture at `size` points. Get the TextureID of one of your wgpu::TextureViews with
    /// GetTextureID, or use the overload in Carbon/Renderer/Render.h that takes the view directly.
    void Image(TextureID texture, Vec2 size, const ImageOptions& options = {});
} // namespace Carbon
