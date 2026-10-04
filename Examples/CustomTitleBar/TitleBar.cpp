#include "TitleBar.h"

#include <algorithm>
#include <cmath>

namespace Example
{
    using namespace Carbon;

    namespace
    {
        // Windows 11 caption buttons are 46 points wide and as tall as the bar.
        constexpr float CaptionButtonWidth = 46.0f;
        // The glyphs inside them are 10 points large.
        constexpr float GlyphSize = 10.0f;
        constexpr float LeadingPadding = 12.0f;
        constexpr float IconSize = 16.0f;
        // Width of the resize handles along the edges, and how far they reach along the edges at the corners.
        constexpr float EdgeThickness = 5.0f;
        constexpr float CornerLength = 12.0f;
        // The close button turns red under the pointer, as on Windows.
        const Color CloseHoverColor = Color::FromHex(0xC42B1C);

        enum class CaptionGlyph
        {
            Minimize,
            Maximize,
            Restore,
            Close
        };

        // The title bar being built: between BeginTitleBar and EndTitleBar.
        TitleBarOptions s_Options;
        TitleBarActions s_Actions;

        Color GetGlyphColor()
        {
            return GetStyleColor(s_Options.IsActive ? StyleColor::Label : StyleColor::TertiaryLabel);
        }

        // The glyphs are drawn as lines one point wide, centered on pixels so that they stay crisp.
        void DrawGlyph(CaptionGlyph glyph, Vec2 center, Color color)
        {
            DrawList& drawList = GetDrawList();
            const ContentScale scale = GetContentScale();
            const float pixels = std::max(1.0f, std::round(scale.Factor));
            const float width = pixels / scale.Factor;
            // An odd number of pixels is centered on a pixel's middle, an even number on its edge.
            const float offset = static_cast<int>(pixels) % 2 == 1 ? scale.GetPixelSize() * 0.5f : 0.0f;
            const Vec2 c = scale.Snap(center) + Vec2(offset, offset);
            const float h = GlyphSize * 0.5f;

            switch (glyph)
            {
                case CaptionGlyph::Minimize:
                    drawList.AddLine(c + Vec2(-h, 0.0f), c + Vec2(h, 0.0f), color, width, false);
                    break;
                case CaptionGlyph::Maximize:
                    drawList.AddSquircleStroke(Rect::FromCenter(c, Vec2(GlyphSize + width)), color, 1.5f, width, 0.0f);
                    break;
                case CaptionGlyph::Restore:
                {
                    // A smaller square in front, and the top and right edges of one behind it.
                    const float front = GlyphSize - 2.0f;
                    const Vec2 frontMin = c + Vec2(-h, -h + 2.0f);
                    drawList.AddSquircleStroke(Rect(frontMin - Vec2(width * 0.5f), Vec2(front + width)), color, 1.0f,
                                               width, 0.0f);
                    const Vec2 backTopLeft = c + Vec2(-h + 2.0f, -h);
                    const Vec2 backTopRight = c + Vec2(h, -h);
                    const Vec2 backBottomRight = c + Vec2(h, h - 2.0f);
                    drawList.AddLine(backTopLeft, backTopRight, color, width);
                    drawList.AddLine(backTopRight, backBottomRight, color, width);
                    drawList.AddLine(backTopLeft, backTopLeft + Vec2(0.0f, 2.0f), color, width);
                    drawList.AddLine(backBottomRight, backBottomRight - Vec2(2.0f, 0.0f), color, width);
                    break;
                }
                case CaptionGlyph::Close:
                    drawList.AddLine(c + Vec2(-h, -h), c + Vec2(h, h), color, width);
                    drawList.AddLine(c + Vec2(h, -h), c + Vec2(-h, h), color, width);
                    break;
            }
        }

        // One caption button: a full-height cell with a glyph. Not a Tab stop: the system's caption buttons are not
        // either, and the window manager offers keyboard commands for them (Alt+Space, Alt+F4).
        bool CaptionButton(std::string_view id, CaptionGlyph glyph, bool isClose)
        {
            const ID buttonID = GetID(id);
            const Rect rect = AllocateItem(Vec2(CaptionButtonWidth, TitleBarHeight));
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            const Interaction interaction = ButtonBehavior(buttonID, rect, behavior);
            const ControlFeedback feedback = AnimateFeedback(buttonID, interaction.Hovered, interaction.Pressed);

            DrawList& drawList = GetDrawList();
            Color glyphColor = GetGlyphColor();
            if (isClose)
            {
                // Red under the pointer, slightly lighter while pressed; the glyph turns white.
                const float amount = std::max(feedback.Hover, feedback.Press);
                drawList.AddRect(rect, CloseHoverColor.WithOpacity(amount * (1.0f - 0.1f * feedback.Press)));
                glyphColor = Lerp(glyphColor, Color::White(), amount);
            }
            else
            {
                const float hover = GetStyleVar(StyleVar::HoverAmount) * feedback.Hover;
                const float opacity = hover + (GetStyleVar(StyleVar::HoverAmount) * 0.6f - hover) * feedback.Press;
                drawList.AddRect(rect, GetStyleColor(StyleColor::Label).WithOpacity(opacity));
            }
            DrawGlyph(glyph, rect.GetCenter(), glyphColor);
            SetLastItem(buttonID, rect, interaction);
            return interaction.Clicked;
        }
    } // namespace

    void BeginTitleBar(const TitleBarOptions& options)
    {
        s_Options = options;
        s_Actions = TitleBarActions();

        // The bar's empty parts move the window. This invisible button is submitted before everything else in the
        // bar, so every control in it wins the pointer over it.
        const Rect bar(GetCursorPos(), Vec2(GetContentRect().Width, TitleBarHeight));
        ButtonBehaviorOptions behavior;
        behavior.Focusable = false;
        behavior.ActivateOnPress = true;
        const Interaction drag = ButtonBehavior(GetID("##titlebar"), bar, behavior);
        if (drag.DoubleClicked)
            s_Actions.ToggleMaximize = true;
        else if (drag.Clicked)
            s_Actions.BeginMove = true;

        BeginHStack({.Spacing = 0.0f,
                     .Width = Size::Fill(),
                     .Height = TitleBarHeight,
                     .Background = GetStyleColor(StyleColor::SecondaryBackground),
                     .CornerRadius = 0.0f,
                     .ID = "##titlebar"});
        Spacer({.Length = LeadingPadding});
        const Color iconColor = s_Options.IsActive ? GetStyleColor(StyleColor::Accent) : GetGlyphColor();
        if (!s_Options.Icon.empty())
        {
            Icon(s_Options.Icon, {.Size = IconSize, .Color = iconColor});
            Spacer({.Length = 8.0f});
        }
        Text(s_Options.Title,
             {.Style = TextStyle::Subheadline,
              .Color = GetStyleColor(s_Options.IsActive ? StyleColor::Label : StyleColor::TertiaryLabel)});
        Spacer({.Length = 20.0f});

        // The toolbar.
        BeginHStack({.Spacing = 8.0f, .Width = Size::Fill(), .ID = "##toolbar"});
    }

    TitleBarActions EndTitleBar()
    {
        EndHStack();
        Spacer({.Length = 12.0f});

        if (CaptionButton("##minimize", CaptionGlyph::Minimize, false))
            s_Actions.Minimize = true;
        Tooltip("Minimize");
        const bool isMaximized = s_Options.IsMaximized;
        if (CaptionButton("##maximize", isMaximized ? CaptionGlyph::Restore : CaptionGlyph::Maximize, false))
            s_Actions.ToggleMaximize = true;
        Tooltip(isMaximized ? "Restore Down" : "Maximize");
        if (CaptionButton("##close", CaptionGlyph::Close, true))
            s_Actions.Close = true;
        Tooltip("Close");
        EndHStack();

        // A hairline separates the bar from the window's content.
        const Rect bar = GetLastItemRect();
        const float pixel = GetContentScale().GetPixelSize();
        GetDrawList().AddRect(Rect(bar.X, bar.GetBottom() - pixel, bar.Width, pixel),
                              GetStyleColor(StyleColor::Separator));
        return s_Actions;
    }

    WindowEdge WindowResizeHandles(bool isEnabled, WindowEdge activeEdges)
    {
        const Vec2 display = GetDisplaySize();
        const float t = EdgeThickness;
        const float c = CornerLength;
        struct Handle
        {
            const char* Id;
            WindowEdge Edges;
            Rect Area;
            Cursor Shape;
        };
        // Edges first, corners last: a corner wins where it overlaps an edge.
        const Handle handles[] = {
            {"##resize-left", WindowEdge::Left, Rect(0.0f, c, t, display.Y - c * 2.0f), Cursor::ResizeHorizontal},
            {"##resize-right", WindowEdge::Right, Rect(display.X - t, c, t, display.Y - c * 2.0f),
             Cursor::ResizeHorizontal},
            {"##resize-top", WindowEdge::Top, Rect(c, 0.0f, display.X - c * 2.0f, t), Cursor::ResizeVertical},
            {"##resize-bottom", WindowEdge::Bottom, Rect(c, display.Y - t, display.X - c * 2.0f, t),
             Cursor::ResizeVertical},
            {"##resize-top-left", WindowEdge::Top | WindowEdge::Left, Rect(0.0f, 0.0f, c, c),
             Cursor::ResizeTopLeftBottomRight},
            {"##resize-top-right", WindowEdge::Top | WindowEdge::Right, Rect(display.X - c, 0.0f, c, c),
             Cursor::ResizeTopRightBottomLeft},
            {"##resize-bottom-left", WindowEdge::Bottom | WindowEdge::Left, Rect(0.0f, display.Y - c, c, c),
             Cursor::ResizeTopRightBottomLeft},
            {"##resize-bottom-right", WindowEdge::Bottom | WindowEdge::Right, Rect(display.X - c, display.Y - c, c, c),
             Cursor::ResizeTopLeftBottomRight},
        };

        WindowEdge started = WindowEdge::None;
        for (const Handle& handle : handles)
        {
            if (activeEdges == handle.Edges)
                SetCursor(handle.Shape);
            if (!isEnabled)
                continue;
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            behavior.ActivateOnPress = true;
            const Interaction interaction = ButtonBehavior(GetID(handle.Id), handle.Area, behavior);
            if (interaction.Hovered)
                SetCursor(handle.Shape);
            if (interaction.Clicked)
                started = handle.Edges;
        }
        return started;
    }
} // namespace Example
