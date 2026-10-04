#pragma once

#include <cstdint>
#include <span>

#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"

namespace Carbon
{
    /// Corner smoothing that matches the look of Apple's continuous corners.
    inline constexpr float DefaultCornerSmoothing = 0.6f;

    /// Identifies a texture a draw command samples. The default value (0) is Carbon's glyph atlas; other values
    /// are handed out by the renderer for host textures.
    struct TextureID
    {
        uint64_t Value = 0;

        constexpr bool operator==(const TextureID& other) const = default;
    };

    /// Drawing order. Later layers draw above earlier ones regardless of submission order.
    enum class DrawLayer : uint8_t
    {
        Background,
        Content,
        /// Popovers, menus, alerts and sheets. This layer is a stack of sub-layers, one per open overlay, so an
        /// overlay opened later always draws above one opened earlier.
        Overlay,
        Tooltip,

        Count
    };

    /// Number of sub-layers of DrawLayer::Overlay: how many overlays can be stacked on top of each other.
    inline constexpr uint32_t MaxOverlayDepth = 8;

    /// How the fragment shader evaluates a primitive.
    enum class DrawPrimitiveKind : uint32_t
    {
        /// Filled squircle (rectangles, circles, pills and lines are special cases).
        Squircle,
        /// Outline of a squircle, StrokeWidth wide, lying inside the shape.
        SquircleStroke,
        /// Soft shadow of a squircle, blurred by Softness.
        Shadow,
        /// Glyph coverage sampled from the atlas.
        Glyph,
        /// Texture sampled and masked by a squircle.
        Image
    };

    /// Shape parameters shared by the four vertices of a quad. Uploaded to the GPU as-is (32 bytes).
    struct DrawPrimitive
    {
        Vec2 HalfSize;
        float Radius = 0.0f;
        float Smoothing = 0.0f;
        float StrokeWidth = 0.0f;
        float Softness = 0.0f;
        DrawPrimitiveKind Kind = DrawPrimitiveKind::Squircle;
        uint32_t Reserved = 0;

        constexpr bool operator==(const DrawPrimitive& other) const = default;
    };

    /// One corner of a quad. Uploaded to the GPU as-is (32 bytes).
    struct DrawVertex
    {
        /// Position in points.
        Vec2 Position;
        /// Position relative to the shape's center, in points; the signed-distance function is evaluated on it.
        Vec2 Local;
        Vec2 UV;
        /// Straight-alpha sRGB color packed by Color::ToRGBA8.
        uint32_t Color = 0;
        /// Index into the primitive array.
        uint32_t Primitive = 0;
    };

    /// Index type of the draw list.
    using DrawIndex = uint32_t;

    /// A run of indices drawn with one clip rectangle and one texture.
    struct DrawCommand
    {
        /// Clip rectangle in points.
        Rect ClipRect;
        TextureID Texture;
        uint32_t IndexOffset = 0;
        uint32_t IndexCount = 0;
    };

    /// A squircle that was added to the draw list before its rectangle was known; see
    /// DrawList::AddDeferredSquircle.
    struct DeferredShape
    {
        uint32_t FirstVertex = 0;
        uint32_t Primitive = 0;
        /// Shadows are drawn shifted by this much.
        Vec2 Offset;
        bool IsValid = false;
    };

    /// Everything a renderer needs to draw one frame. The spans stay valid until the next NewFrame.
    struct DrawData
    {
        std::span<const DrawVertex> Vertices;
        std::span<const DrawIndex> Indices;
        std::span<const DrawPrimitive> Primitives;
        /// Commands in drawing order (layers already merged back to front).
        std::span<const DrawCommand> Commands;
        /// Display size in points.
        Vec2 DisplaySize;
        /// Pixels per point.
        float ContentScale = 1.0f;
    };
} // namespace Carbon
