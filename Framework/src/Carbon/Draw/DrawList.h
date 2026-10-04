#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "Carbon/Core/Color.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"

namespace Carbon
{
    struct TextSpec;

    /// Collects the shapes of one frame as quads plus per-shape parameters. It is GPU-free: the renderer uploads
    /// its output, and tests inspect it directly. All coordinates are in points.
    ///
    /// Every shape is one quad whose fragment shader evaluates a signed-distance function, so edges are
    /// antialiased analytically at any content scale. Shapes entirely outside the clip rectangle are dropped.
    class DrawList
    {
    public:
        DrawList();

        /// Clears all geometry and resets the stacks. Called by NewFrame.
        void Reset(const Rect& displayRect, float contentScale);

        /// Redirects following shapes to a layer until PopLayer. Each layer has its own clip stack.
        void PushLayer(DrawLayer layer);
        void PopLayer();
        DrawLayer GetLayer() const { return m_LayerStack.back(); }

        /// Restricts following shapes to a rectangle, by default intersected with the current one.
        void PushClipRect(const Rect& rect, bool intersectWithCurrent = true);
        void PopClipRect();
        const Rect& GetClipRect() const;

        /// Multiplies the alpha of following shapes; nests multiplicatively.
        void PushOpacity(float opacity);
        void PopOpacity();
        float GetOpacity() const { return m_OpacityStack.back(); }

        /// Pixels per point of the frame being drawn.
        float GetContentScale() const { return m_ContentScale; }

        /// Filled rectangle with sharp corners.
        void AddRect(const Rect& rect, Color color);
        /// Filled rectangle with continuous-curvature corners. Smoothing 0 gives circular arcs.
        void AddSquircle(const Rect& rect, Color color, float radius, float smoothing = DefaultCornerSmoothing);
        /// Outline of a squircle, `width` wide, drawn inside `rect`.
        void AddSquircleStroke(const Rect& rect, Color color, float radius, float width,
                               float smoothing = DefaultCornerSmoothing);
        /// Ring around a control: a stroke `width` wide whose inner edge is `offset` outside `rect`, with corners
        /// concentric to the control's.
        void AddFocusRing(const Rect& rect, Color color, float radius, float width, float offset,
                          float smoothing = DefaultCornerSmoothing);
        /// Filled circle.
        void AddCircle(Vec2 center, float radius, Color color);
        /// Outline of a circle, `width` wide, drawn inside the radius.
        void AddCircleStroke(Vec2 center, float radius, Color color, float width);
        /// Line segment `width` wide, with round or flat ends.
        void AddLine(Vec2 from, Vec2 to, Color color, float width, bool roundCaps = true);
        /// Soft shadow of a squircle. `blur` is the width of the falloff in points.
        void AddShadow(const Rect& rect, Color color, float radius, float blur, Vec2 offset = Vec2(),
                       float smoothing = DefaultCornerSmoothing);
        /// Texture drawn into `rect`, optionally with squircle corners. `uv` is the sampled region (0..1).
        void AddImage(TextureID texture, const Rect& rect, const Rect& uv = Rect(0.0f, 0.0f, 1.0f, 1.0f),
                      Color tint = Color::White(), float radius = 0.0f, float smoothing = DefaultCornerSmoothing);
        /// Text with its top-left corner at `position`. Lines are separated by '\n'. Glyphs are placed on whole
        /// pixels, so text stays crisp at any content scale.
        void AddText(Vec2 position, std::string_view text, const TextSpec& spec, Color color);
        /// One glyph from the glyph atlas. Used by the text layer; `uv` is the glyph's atlas region in texels.
        void AddGlyph(const Rect& rect, const Rect& uv, Color color);

        std::span<const DrawVertex> GetVertices() const { return m_Vertices; }
        std::span<const DrawPrimitive> GetPrimitives() const { return m_Primitives; }
        std::span<const DrawIndex> GetIndices(DrawLayer layer) const;
        std::span<const DrawCommand> GetCommands(DrawLayer layer) const;

        /// True when every PushLayer, PushClipRect and PushOpacity has been popped.
        bool IsBalanced() const;

        /// Merges the layers back to front and returns the frame's draw data. Called by EndFrame.
        const DrawData& Finalize();
        /// The draw data produced by the last Finalize.
        const DrawData& GetDrawData() const { return m_DrawData; }

    private:
        struct LayerData
        {
            std::vector<DrawIndex> Indices;
            std::vector<DrawCommand> Commands;
            std::vector<Rect> ClipStack;
            /// False while the current command contains only untextured shapes, which can share any texture.
            bool CommandUsesTexture = false;
        };

        LayerData& GetLayerData() { return m_Layers[static_cast<size_t>(m_LayerStack.back())]; }
        const LayerData& GetLayerData() const { return m_Layers[static_cast<size_t>(m_LayerStack.back())]; }

        bool IsVisible(const Rect& bounds, Color color) const;
        uint32_t AddPrimitive(const DrawPrimitive& primitive);
        void AddShape(const Rect& rect, Color color, const DrawPrimitive& primitive, float expand);
        void AddQuad(const Vec2 positions[4], const Vec2 locals[4], const Vec2 uvs[4], Color color, uint32_t primitive,
                     TextureID texture, bool usesTexture);
        DrawCommand& GetCommand(LayerData& layer, TextureID texture, bool usesTexture);

    private:
        std::vector<DrawVertex> m_Vertices;
        std::vector<DrawPrimitive> m_Primitives;
        std::array<LayerData, static_cast<size_t>(DrawLayer::Count)> m_Layers;
        std::vector<DrawLayer> m_LayerStack;
        std::vector<float> m_OpacityStack;
        std::vector<DrawIndex> m_MergedIndices;
        std::vector<DrawCommand> m_MergedCommands;
        DrawData m_DrawData;
        Rect m_DisplayRect;
        float m_ContentScale = 1.0f;
        /// Extra margin around every shape, in points, that leaves room for the antialiased edge.
        float m_Padding = 1.0f;
        uint32_t m_GlyphPrimitive = 0;
        bool m_HasGlyphPrimitive = false;
    };

    /// Returns the draw list of the current frame. Custom components draw their shapes through it.
    DrawList& GetDrawList();
} // namespace Carbon
