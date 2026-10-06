#include "Carbon/Draw/DrawList.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"

namespace Carbon
{
    namespace
    {
        // A corner radius can be at most half of the shorter side. Empty and inverted rectangles get no radius.
        float ClampRadius(float radius, Vec2 halfSize)
        {
            return std::clamp(radius, 0.0f, std::max(0.0f, std::min(halfSize.X, halfSize.Y)));
        }
    } // namespace

    DrawList::DrawList()
    {
        Reset(Rect(), 1.0f);
    }

    void DrawList::Reset(const Rect& displayRect, float contentScale)
    {
        m_DisplayRect = displayRect;
        m_ContentScale = contentScale > 0.0f ? contentScale : 1.0f;
        // One physical pixel of margin is enough for the antialiased edge; never less than one point.
        m_Padding = std::max(1.0f, 1.0f / m_ContentScale);

        m_Vertices.clear();
        m_Primitives.clear();
        for (LayerData& layer : m_Layers)
        {
            layer.Indices.clear();
            layer.Commands.clear();
            layer.ClipStack.clear();
            layer.ClipStack.push_back(displayRect);
            layer.CommandUsesTexture = false;
        }
        m_LayerStack.clear();
        m_LayerStack.push_back(GetLayerSlot(DrawLayer::Content, 0));
        m_OpacityStack.clear();
        m_OpacityStack.push_back(1.0f);
        m_MergedIndices.clear();
        m_MergedCommands.clear();
        m_DrawData = DrawData();
        m_HasGlyphPrimitive = false;
    }

    void DrawList::PushLayer(DrawLayer layer, uint32_t depth)
    {
        CB_VERIFY(layer < DrawLayer::Count, "Invalid draw layer {}", static_cast<int>(layer));
        m_LayerStack.push_back(GetLayerSlot(layer < DrawLayer::Count ? layer : DrawLayer::Content, depth));
    }

    void DrawList::PopLayer()
    {
        CB_VERIFY(m_LayerStack.size() > 1, "PopLayer called without a matching PushLayer");
        if (m_LayerStack.size() > 1)
            m_LayerStack.pop_back();
    }

    DrawLayer DrawList::GetLayer() const
    {
        const uint8_t slot = m_LayerStack.back();
        const uint8_t firstOverlay = GetLayerSlot(DrawLayer::Overlay, 0);
        if (slot < firstOverlay)
            return static_cast<DrawLayer>(slot);
        return slot < firstOverlay + MaxOverlayDepth ? DrawLayer::Overlay : DrawLayer::Tooltip;
    }

    void DrawList::PushClipRect(const Rect& rect, bool intersectWithCurrent)
    {
        LayerData& layer = GetLayerData();
        layer.ClipStack.push_back(intersectWithCurrent ? layer.ClipStack.back().GetIntersection(rect) : rect);
    }

    void DrawList::PopClipRect()
    {
        LayerData& layer = GetLayerData();
        CB_VERIFY(layer.ClipStack.size() > 1, "PopClipRect called without a matching PushClipRect");
        if (layer.ClipStack.size() > 1)
            layer.ClipStack.pop_back();
    }

    const Rect& DrawList::GetClipRect() const
    {
        return GetLayerData().ClipStack.back();
    }

    void DrawList::PushOpacity(float opacity, bool inherit)
    {
        m_OpacityStack.push_back((inherit ? m_OpacityStack.back() : 1.0f) * std::clamp(opacity, 0.0f, 1.0f));
    }

    void DrawList::PopOpacity()
    {
        CB_VERIFY(m_OpacityStack.size() > 1, "PopOpacity called without a matching PushOpacity");
        if (m_OpacityStack.size() > 1)
            m_OpacityStack.pop_back();
    }

    void DrawList::AddRect(const Rect& rect, Color color)
    {
        AddSquircle(rect, color, 0.0f, 0.0f);
    }

    void DrawList::AddSquircle(const Rect& rect, Color color, float radius, float smoothing)
    {
        DrawPrimitive primitive;
        primitive.HalfSize = rect.GetSize() * 0.5f;
        primitive.Radius = ClampRadius(radius, primitive.HalfSize);
        primitive.Smoothing = std::clamp(smoothing, 0.0f, 1.0f);
        primitive.Kind = DrawPrimitiveKind::Squircle;
        AddShape(rect, color, primitive, m_Padding);
    }

    void DrawList::AddSquircleStroke(const Rect& rect, Color color, float radius, float width, float smoothing)
    {
        if (width <= 0.0f)
            return;
        DrawPrimitive primitive;
        primitive.HalfSize = rect.GetSize() * 0.5f;
        primitive.Radius = ClampRadius(radius, primitive.HalfSize);
        primitive.Smoothing = std::clamp(smoothing, 0.0f, 1.0f);
        primitive.StrokeWidth = width;
        primitive.Kind = DrawPrimitiveKind::SquircleStroke;
        AddShape(rect, color, primitive, m_Padding);
    }

    void DrawList::AddFocusRing(const Rect& rect, Color color, float radius, float width, float offset, float smoothing)
    {
        const float outset = offset + width;
        AddSquircleStroke(rect.Expand(outset), color, std::max(0.0f, radius + outset), width, smoothing);
    }

    void DrawList::AddCircle(Vec2 center, float radius, Color color)
    {
        AddSquircle(Rect::FromCenter(center, Vec2(radius * 2.0f)), color, radius, 0.0f);
    }

    void DrawList::AddCircleStroke(Vec2 center, float radius, Color color, float width)
    {
        AddSquircleStroke(Rect::FromCenter(center, Vec2(radius * 2.0f)), color, radius, width, 0.0f);
    }

    void DrawList::AddLine(Vec2 from, Vec2 to, Color color, float width, bool roundCaps)
    {
        if (width <= 0.0f)
            return;

        const Vec2 delta = to - from;
        const float length = delta.GetLength();
        const Vec2 along = length > 1e-6f ? delta / length : Vec2(1.0f, 0.0f);
        const Vec2 across = Vec2(-along.Y, along.X);
        const float halfWidth = width * 0.5f;
        const float capLength = roundCaps ? halfWidth : 0.0f;

        // A line is a rotated pill: the quad is oriented along the segment and `Local` is in the line's frame.
        DrawPrimitive primitive;
        primitive.HalfSize = Vec2(length * 0.5f + capLength, halfWidth);
        primitive.Radius = roundCaps ? halfWidth : 0.0f;
        primitive.Kind = DrawPrimitiveKind::Squircle;

        const Vec2 center = (from + to) * 0.5f;
        const Vec2 extent = primitive.HalfSize + Vec2(m_Padding);
        const Vec2 signs[4] = {Vec2(-1.0f, -1.0f), Vec2(1.0f, -1.0f), Vec2(1.0f, 1.0f), Vec2(-1.0f, 1.0f)};
        Vec2 positions[4];
        Vec2 locals[4];
        Vec2 uvs[4];
        Vec2 boundsMin = center;
        Vec2 boundsMax = center;
        for (int i = 0; i < 4; i++)
        {
            locals[i] = extent * signs[i];
            positions[i] = center + along * locals[i].X + across * locals[i].Y;
            boundsMin = Min(boundsMin, positions[i]);
            boundsMax = Max(boundsMax, positions[i]);
        }

        if (!IsVisible(Rect::FromMinMax(boundsMin, boundsMax), color))
            return;
        AddQuad(positions, locals, uvs, color, AddPrimitive(primitive), TextureID(), false);
    }

    void DrawList::AddShadow(const Rect& rect, Color color, float radius, float blur, Vec2 offset, float smoothing)
    {
        const Rect shape = rect.Offset(offset);
        DrawPrimitive primitive;
        primitive.HalfSize = shape.GetSize() * 0.5f;
        primitive.Radius = ClampRadius(radius, primitive.HalfSize);
        primitive.Smoothing = std::clamp(smoothing, 0.0f, 1.0f);
        primitive.Softness = std::max(blur, 0.0f);
        primitive.Kind = DrawPrimitiveKind::Shadow;
        AddShape(shape, color, primitive, primitive.Softness + m_Padding);
    }

    void DrawList::AddImage(TextureID texture, const Rect& rect, const Rect& uv, Color tint, float radius,
                            float smoothing)
    {
        if (rect.IsEmpty())
            return;
        const Rect bounds = rect.Expand(m_Padding);
        if (!IsVisible(bounds, tint))
            return;

        DrawPrimitive primitive;
        primitive.HalfSize = rect.GetSize() * 0.5f;
        primitive.Radius = ClampRadius(radius, primitive.HalfSize);
        primitive.Smoothing = std::clamp(smoothing, 0.0f, 1.0f);
        primitive.Kind = DrawPrimitiveKind::Image;

        // The quad is larger than the image by the antialiasing margin, so the UVs extrapolate accordingly.
        const Vec2 uvPerPoint = uv.GetSize() / rect.GetSize();
        const Vec2 uvMin = uv.GetMin() - uvPerPoint * m_Padding;
        const Vec2 uvMax = uv.GetMax() + uvPerPoint * m_Padding;
        const Vec2 center = rect.GetCenter();
        const Vec2 positions[4] = {bounds.GetMin(), Vec2(bounds.GetRight(), bounds.Y), bounds.GetMax(),
                                   Vec2(bounds.X, bounds.GetBottom())};
        const Vec2 uvs[4] = {uvMin, Vec2(uvMax.X, uvMin.Y), uvMax, Vec2(uvMin.X, uvMax.Y)};
        Vec2 locals[4];
        for (int i = 0; i < 4; i++)
            locals[i] = positions[i] - center;
        AddQuad(positions, locals, uvs, tint, AddPrimitive(primitive), texture, true);
    }

    void DrawList::AddGlyph(const Rect& rect, const Rect& uv, Color color)
    {
        if (!IsVisible(rect, color))
            return;

        // All glyphs of a frame share one primitive; they need no shape parameters.
        if (!m_HasGlyphPrimitive)
        {
            DrawPrimitive primitive;
            primitive.Kind = DrawPrimitiveKind::Glyph;
            m_GlyphPrimitive = static_cast<uint32_t>(m_Primitives.size());
            m_Primitives.push_back(primitive);
            m_HasGlyphPrimitive = true;
        }

        const Vec2 positions[4] = {rect.GetMin(), Vec2(rect.GetRight(), rect.Y), rect.GetMax(),
                                   Vec2(rect.X, rect.GetBottom())};
        const Vec2 uvs[4] = {uv.GetMin(), Vec2(uv.GetRight(), uv.Y), uv.GetMax(), Vec2(uv.X, uv.GetBottom())};
        const Vec2 locals[4] = {};
        AddQuad(positions, locals, uvs, color, m_GlyphPrimitive, TextureID(), true);
    }

    DeferredShape DrawList::AddDeferredSquircle(Color color)
    {
        DrawPrimitive primitive;
        primitive.Kind = DrawPrimitiveKind::Squircle;
        return AddDeferredShape(color, primitive);
    }

    DeferredShape DrawList::AddDeferredSquircleStroke(Color color, float width)
    {
        if (width <= 0.0f)
            return DeferredShape();
        DrawPrimitive primitive;
        primitive.StrokeWidth = width;
        primitive.Kind = DrawPrimitiveKind::SquircleStroke;
        return AddDeferredShape(color, primitive);
    }

    DeferredShape DrawList::AddDeferredShadow(Color color, float blur, Vec2 offset)
    {
        DrawPrimitive primitive;
        primitive.Softness = std::max(blur, 0.0f);
        primitive.Kind = DrawPrimitiveKind::Shadow;
        DeferredShape shape = AddDeferredShape(color, primitive);
        shape.Offset = offset;
        return shape;
    }

    DeferredShape DrawList::AddDeferredShape(Color color, const DrawPrimitive& description)
    {
        DeferredShape shape;
        if (color.A * m_OpacityStack.back() <= 0.0f)
            return shape;

        // The primitive is marked as reserved so that no other shape shares it while its values are pending.
        DrawPrimitive primitive = description;
        primitive.Reserved = 1;
        shape.Primitive = static_cast<uint32_t>(m_Primitives.size());
        m_Primitives.push_back(primitive);

        // A degenerate quad for now; clipping is left to the scissor rectangle of the command.
        shape.FirstVertex = static_cast<uint32_t>(m_Vertices.size());
        const Vec2 zero[4] = {};
        AddQuad(zero, zero, zero, color, shape.Primitive, TextureID(), false);
        shape.IsValid = true;
        return shape;
    }

    void DrawList::ResolveDeferredSquircle(const DeferredShape& shape, const Rect& rect, float radius, float smoothing)
    {
        if (!shape.IsValid || rect.IsEmpty() || static_cast<size_t>(shape.FirstVertex) + 4 > m_Vertices.size())
            return;

        const Rect placed = rect.Offset(shape.Offset);
        DrawPrimitive& primitive = m_Primitives[shape.Primitive];
        primitive.HalfSize = placed.GetSize() * 0.5f;
        primitive.Radius = ClampRadius(radius, primitive.HalfSize);
        primitive.Smoothing = std::clamp(smoothing, 0.0f, 1.0f);
        primitive.Reserved = 0;

        const Rect bounds = placed.Expand(m_Padding + primitive.Softness);
        const Vec2 center = placed.GetCenter();
        const Vec2 positions[4] = {bounds.GetMin(), Vec2(bounds.GetRight(), bounds.Y), bounds.GetMax(),
                                   Vec2(bounds.X, bounds.GetBottom())};
        for (uint32_t i = 0; i < 4; i++)
        {
            DrawVertex& vertex = m_Vertices[shape.FirstVertex + i];
            vertex.Position = positions[i];
            vertex.Local = positions[i] - center;
        }
    }

    std::span<const DrawIndex> DrawList::GetIndices(DrawLayer layer, uint32_t depth) const
    {
        return m_Layers[GetLayerSlot(layer, depth)].Indices;
    }

    std::span<const DrawCommand> DrawList::GetCommands(DrawLayer layer, uint32_t depth) const
    {
        return m_Layers[GetLayerSlot(layer, depth)].Commands;
    }

    uint8_t DrawList::GetLayerSlot(DrawLayer layer, uint32_t depth)
    {
        switch (layer)
        {
            case DrawLayer::Background:
                return 0;
            case DrawLayer::Overlay:
                return static_cast<uint8_t>(2 + std::min(depth, MaxOverlayDepth - 1));
            case DrawLayer::Tooltip:
                return static_cast<uint8_t>(2 + MaxOverlayDepth);
            default:
                return 1;
        }
    }

    bool DrawList::IsBalanced() const
    {
        if (m_LayerStack.size() != 1 || m_OpacityStack.size() != 1)
            return false;
        for (const LayerData& layer : m_Layers)
        {
            if (layer.ClipStack.size() != 1)
                return false;
        }
        return true;
    }

    const DrawData& DrawList::Finalize()
    {
        m_MergedIndices.clear();
        m_MergedCommands.clear();
        for (const LayerData& layer : m_Layers)
        {
            const uint32_t indexBase = static_cast<uint32_t>(m_MergedIndices.size());
            m_MergedIndices.insert(m_MergedIndices.end(), layer.Indices.begin(), layer.Indices.end());
            for (const DrawCommand& command : layer.Commands)
            {
                if (command.IndexCount == 0)
                    continue;
                DrawCommand merged = command;
                merged.IndexOffset += indexBase;
                m_MergedCommands.push_back(merged);
            }
        }

        m_DrawData.Vertices = m_Vertices;
        m_DrawData.Indices = m_MergedIndices;
        m_DrawData.Primitives = m_Primitives;
        m_DrawData.Commands = m_MergedCommands;
        m_DrawData.DisplaySize = m_DisplayRect.GetSize();
        m_DrawData.ContentScale = m_ContentScale;
        return m_DrawData;
    }

    bool DrawList::IsVisible(const Rect& bounds, Color color) const
    {
        if (color.A * m_OpacityStack.back() <= 0.0f)
            return false;
        return GetClipRect().Intersects(bounds);
    }

    uint32_t DrawList::AddPrimitive(const DrawPrimitive& primitive)
    {
        // Runs of identical shapes (list rows, separators) share one primitive.
        if (!m_Primitives.empty() && m_Primitives.back() == primitive)
            return static_cast<uint32_t>(m_Primitives.size() - 1);
        m_Primitives.push_back(primitive);
        return static_cast<uint32_t>(m_Primitives.size() - 1);
    }

    void DrawList::AddShape(const Rect& rect, Color color, const DrawPrimitive& primitive, float expand)
    {
        if (rect.IsEmpty())
            return;
        const Rect bounds = rect.Expand(expand);
        if (!IsVisible(bounds, color))
            return;

        const Vec2 center = rect.GetCenter();
        const Vec2 positions[4] = {bounds.GetMin(), Vec2(bounds.GetRight(), bounds.Y), bounds.GetMax(),
                                   Vec2(bounds.X, bounds.GetBottom())};
        const Vec2 uvs[4] = {};
        Vec2 locals[4];
        for (int i = 0; i < 4; i++)
            locals[i] = positions[i] - center;
        AddQuad(positions, locals, uvs, color, AddPrimitive(primitive), TextureID(), false);
    }

    void DrawList::AddQuad(const Vec2 positions[4], const Vec2 locals[4], const Vec2 uvs[4], Color color,
                           uint32_t primitive, TextureID texture, bool usesTexture)
    {
        LayerData& layer = GetLayerData();
        DrawCommand& command = GetCommand(layer, texture, usesTexture);

        // The storage for the quad is made in one step and then written in place: this runs for every glyph.
        const uint32_t packedColor = color.WithOpacity(m_OpacityStack.back()).ToRGBA8();
        const size_t firstVertex = m_Vertices.size();
        m_Vertices.resize(firstVertex + 4);
        DrawVertex* vertices = m_Vertices.data() + firstVertex;
        for (int i = 0; i < 4; i++)
        {
            vertices[i].Position = positions[i];
            vertices[i].Local = locals[i];
            vertices[i].UV = uvs[i];
            vertices[i].Color = packedColor;
            vertices[i].Primitive = primitive;
        }

        const DrawIndex base = static_cast<DrawIndex>(firstVertex);
        const size_t firstIndex = layer.Indices.size();
        layer.Indices.resize(firstIndex + 6);
        DrawIndex* indices = layer.Indices.data() + firstIndex;
        indices[0] = base;
        indices[1] = base + 1;
        indices[2] = base + 2;
        indices[3] = base;
        indices[4] = base + 2;
        indices[5] = base + 3;
        command.IndexCount += 6;
    }

    DrawCommand& DrawList::GetCommand(LayerData& layer, TextureID texture, bool usesTexture)
    {
        const Rect& clipRect = layer.ClipStack.back();
        if (!layer.Commands.empty())
        {
            DrawCommand& current = layer.Commands.back();
            if (current.ClipRect == clipRect)
            {
                // Untextured shapes never sample, so they fit into any command.
                if (!usesTexture)
                    return current;
                if (!layer.CommandUsesTexture)
                {
                    current.Texture = texture;
                    layer.CommandUsesTexture = true;
                    return current;
                }
                if (current.Texture == texture)
                    return current;
            }
        }

        DrawCommand command;
        command.ClipRect = clipRect;
        command.Texture = usesTexture ? texture : TextureID();
        command.IndexOffset = static_cast<uint32_t>(layer.Indices.size());
        layer.Commands.push_back(command);
        layer.CommandUsesTexture = usesTexture;
        return layer.Commands.back();
    }

    DrawList& GetDrawList()
    {
        return Internal::GetFrameContext().Draw;
    }
} // namespace Carbon
