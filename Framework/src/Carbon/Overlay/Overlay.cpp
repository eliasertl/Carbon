#include "Carbon/Overlay/Overlay.h"

#include <algorithm>
#include <cstdint>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/LayoutInternal.h"
#include "Carbon/Overlay/OverlayInternal.h"

namespace Carbon
{
    namespace
    {
        using Internal::OpenOverlayEntry;
        using Internal::OverlayBuild;
        using Internal::OverlayState;

        constexpr size_t NotOpen = SIZE_MAX;
        // Overlays keep this distance from the edges of the display.
        constexpr float ScreenMargin = 8.0f;
        // How far a popover's arrow sticks out; its base is twice as wide.
        constexpr float ArrowLength = 7.0f;
        // Alerts and sheets settle into place from this far above.
        constexpr float AppearTravel = 12.0f;
        constexpr float ShadowBlur = 24.0f;
        constexpr Vec2 ShadowOffset(0.0f, 8.0f);

        size_t FindOpen(const OverlayState& state, ID id)
        {
            for (size_t i = 0; i < state.Open.size(); i++)
            {
                if (state.Open[i].Id == id)
                    return i;
            }
            return NotOpen;
        }

        // Closes the overlay at `index` and everything above it.
        void CloseFrom(Context& context, size_t index)
        {
            OverlayState& state = context.Overlays;
            // Focus returns to where it was before the lowest of the closing overlays took the keyboard.
            for (size_t i = index; i < state.Open.size(); i++)
            {
                const OpenOverlayEntry& entry = state.Open[i];
                if (entry.IsStarted && entry.Captures)
                {
                    context.Interaction.FocusedID = entry.PreviousFocus;
                    context.Interaction.IsFocusVisible = entry.WasFocusVisible;
                    context.Interaction.FocusSetFrame = context.FrameCount;
                    break;
                }
            }
            state.Open.erase(state.Open.begin() + static_cast<std::ptrdiff_t>(index), state.Open.end());
            context.IsAnimatingThisFrame = true;
        }

        float GetFactor(Alignment alignment)
        {
            return alignment == Alignment::Leading ? 0.0f : (alignment == Alignment::Center ? 0.5f : 1.0f);
        }

        struct PlacedOverlay
        {
            Vec2 Origin;
            OverlayPlacement Placement = OverlayPlacement::Below;
        };

        PlacedOverlay PlaceOverlay(const OverlayOptions& options, Vec2 size, Vec2 display, float arrow)
        {
            const Rect& anchor = options.Anchor;
            const float gap = options.Gap + arrow;
            const float factor = GetFactor(options.Alignment);

            // Flip to the opposite side when the preferred one lacks room and the other one has more.
            PlacedOverlay placed;
            placed.Placement = options.Placement;
            const float roomBelow = display.Y - ScreenMargin - anchor.GetBottom() - gap;
            const float roomAbove = anchor.Y - gap - ScreenMargin;
            const float roomTrailing = display.X - ScreenMargin - anchor.GetRight() - gap;
            const float roomLeading = anchor.X - gap - ScreenMargin;
            switch (options.Placement)
            {
                case OverlayPlacement::Below:
                    if (size.Y > roomBelow && roomAbove > roomBelow)
                        placed.Placement = OverlayPlacement::Above;
                    break;
                case OverlayPlacement::Above:
                    if (size.Y > roomAbove && roomBelow > roomAbove)
                        placed.Placement = OverlayPlacement::Below;
                    break;
                case OverlayPlacement::Trailing:
                    if (size.X > roomTrailing && roomLeading > roomTrailing)
                        placed.Placement = OverlayPlacement::Leading;
                    break;
                case OverlayPlacement::Leading:
                    if (size.X > roomLeading && roomTrailing > roomLeading)
                        placed.Placement = OverlayPlacement::Trailing;
                    break;
                default:
                    break;
            }

            const float alignedX = anchor.X + (anchor.Width - size.X) * factor;
            const float alignedY = anchor.Y + (anchor.Height - size.Y) * factor;
            switch (placed.Placement)
            {
                case OverlayPlacement::Below:
                    placed.Origin = Vec2(alignedX, anchor.GetBottom() + gap);
                    break;
                case OverlayPlacement::Above:
                    placed.Origin = Vec2(alignedX, anchor.Y - gap - size.Y);
                    break;
                case OverlayPlacement::Trailing:
                    placed.Origin = Vec2(anchor.GetRight() + gap, alignedY);
                    break;
                case OverlayPlacement::Leading:
                    placed.Origin = Vec2(anchor.X - gap - size.X, alignedY);
                    break;
                case OverlayPlacement::Center:
                    placed.Origin = (display - size) * 0.5f;
                    break;
                case OverlayPlacement::Top:
                    placed.Origin = Vec2((display.X - size.X) * 0.5f, options.Gap);
                    break;
            }

            // Whatever the anchor says, the overlay stays on the display.
            if (options.Placement != OverlayPlacement::Top)
            {
                placed.Origin.X = std::clamp(placed.Origin.X, ScreenMargin,
                                             std::max(ScreenMargin, display.X - size.X - ScreenMargin));
                placed.Origin.Y = std::clamp(placed.Origin.Y, ScreenMargin,
                                             std::max(ScreenMargin, display.Y - size.Y - ScreenMargin));
            }
            return placed;
        }

        // The arrow is a square standing on one corner, half of it hidden behind the overlay's edge.
        void DrawArrow(Context& context, const OverlayBuild& build, const Rect& rect)
        {
            Vec2 normal;
            Vec2 tangent;
            Vec2 base;
            const float inset = build.Radius + ArrowLength + 2.0f;
            const Vec2 target = build.Anchor.GetCenter();
            const auto along = [inset](float value, float start, float length)
            {
                const float low = start + inset;
                const float high = start + length - inset;
                return low <= high ? std::clamp(value, low, high) : start + length * 0.5f;
            };
            switch (build.Placement)
            {
                case OverlayPlacement::Below:
                    normal = Vec2(0.0f, -1.0f);
                    tangent = Vec2(1.0f, 0.0f);
                    base = Vec2(along(target.X, rect.X, rect.Width), rect.Y);
                    break;
                case OverlayPlacement::Above:
                    normal = Vec2(0.0f, 1.0f);
                    tangent = Vec2(1.0f, 0.0f);
                    base = Vec2(along(target.X, rect.X, rect.Width), rect.GetBottom());
                    break;
                case OverlayPlacement::Trailing:
                    normal = Vec2(-1.0f, 0.0f);
                    tangent = Vec2(0.0f, 1.0f);
                    base = Vec2(rect.X, along(target.Y, rect.Y, rect.Height));
                    break;
                case OverlayPlacement::Leading:
                    normal = Vec2(1.0f, 0.0f);
                    tangent = Vec2(0.0f, 1.0f);
                    base = Vec2(rect.GetRight(), along(target.Y, rect.Y, rect.Height));
                    break;
                default:
                    return;
            }

            DrawList& drawList = context.Draw;
            const Color fill = context.Style.GetColor(StyleColor::OverlayBackground);
            const Color border = context.Style.GetColor(StyleColor::OverlayBorder);
            const float pixel = context.Scale.GetPixelSize();
            const float length = ArrowLength;
            const Vec2 tip = base + normal * length;
            const Vec2 left = base - tangent * length;
            const Vec2 right = base + tangent * length;

            // Only the half outside the overlay is drawn, so nothing is painted twice while the overlay fades in.
            const Vec2 clipFrom = base - tangent * (length + 1.0f);
            const Vec2 clipTo = base + tangent * (length + 1.0f) + normal * (length + 1.0f);
            drawList.PushClipRect(Rect::FromMinMax(Min(clipFrom, clipTo), Max(clipFrom, clipTo)));
            const float side = length * 1.41421356f;
            const Vec2 diagonal = (normal + tangent) * 0.70710678f;
            drawList.AddLine(base - diagonal * (side * 0.5f), base + diagonal * (side * 0.5f), fill, side, false);
            drawList.AddLine(left, tip, border, pixel);
            drawList.AddLine(tip, right, border, pixel);
            drawList.PopClipRect();

            // The overlay's outline is interrupted where the arrow joins it.
            const Vec2 inside = normal * (pixel * -0.5f);
            drawList.AddLine(left + tangent * pixel + inside, right - tangent * pixel + inside, fill, pixel, false);
        }
    } // namespace

    namespace Internal
    {
        void BeginOverlays(Context& context)
        {
            context.Overlays.Building.clear();
            context.Overlays.IsEscapeConsumed = false;
        }

        void EndOverlays(Context& context)
        {
            OverlayState& state = context.Overlays;
            state.Building.clear();
            // An overlay opened this frame may not have reached its BeginOverlay yet.
            for (size_t i = 0; i < state.Open.size(); i++)
            {
                const OpenOverlayEntry& entry = state.Open[i];
                if (entry.LastFrame != context.FrameCount && entry.OpenedFrame != context.FrameCount)
                {
                    CloseFrom(context, i);
                    break;
                }
            }
        }

        void AbandonOverlay(Context& context)
        {
            context.Draw.PopOpacity();
            context.Draw.PopLayer();
            if (context.IDStack.size() > 1)
                context.IDStack.pop_back();
            if (!context.Overlays.Building.empty())
                context.Overlays.Building.pop_back();
        }

        ID GetActiveFocusScope(const Context& context)
        {
            const std::vector<OpenOverlayEntry>& open = context.Overlays.Open;
            for (size_t i = open.size(); i > 0; i--)
            {
                if (open[i - 1].IsStarted && open[i - 1].Captures)
                    return open[i - 1].Id;
            }
            return ID();
        }

        ID GetCurrentFocusScope(const Context& context)
        {
            const std::vector<OverlayBuild>& building = context.Overlays.Building;
            for (size_t i = building.size(); i > 0; i--)
            {
                if (building[i - 1].Captures)
                    return building[i - 1].Id;
            }
            return ID();
        }

        bool IsInActiveFocusScope(const Context& context)
        {
            return GetCurrentFocusScope(context) == GetActiveFocusScope(context);
        }

        bool IsPointerBlockedByOverlay(const Context& context)
        {
            return IsPointBlockedByOverlay(context, context.Input.MousePos);
        }

        bool IsPointBlockedByOverlay(const Context& context, Vec2 point)
        {
            const std::vector<OpenOverlayEntry>& open = context.Overlays.Open;
            if (open.empty())
                return false;
            const uint32_t order = context.Draw.GetLayerOrder();
            for (size_t i = 0; i < open.size(); i++)
            {
                const OpenOverlayEntry& entry = open[i];
                if (!entry.IsStarted || DrawList::GetLayerOrder(DrawLayer::Overlay, static_cast<uint32_t>(i)) <= order)
                    continue;
                if (entry.Captures || entry.Bounds.Contains(point))
                    return true;
            }
            return false;
        }
    } // namespace Internal

    void OpenOverlay(ID id)
    {
        Context& context = Internal::GetContext();
        CB_VERIFY(id.IsValid(), "OpenOverlay needs a valid ID");
        if (!id.IsValid() || FindOpen(context.Overlays, id) != NotOpen)
            return;

        OpenOverlayEntry entry;
        entry.Id = id;
        entry.OpenedFrame = context.FrameCount;
        context.Overlays.Open.push_back(entry);
        context.IsAnimatingThisFrame = true;
    }

    void CloseOverlay(ID id)
    {
        Context& context = Internal::GetContext();
        const size_t index = FindOpen(context.Overlays, id);
        if (index != NotOpen)
            CloseFrom(context, index);
    }

    void CloseCurrentOverlay()
    {
        Context& context = Internal::GetFrameContext();
        const bool isBuilding = !context.Overlays.Building.empty();
        CB_VERIFY(isBuilding, "CloseCurrentOverlay must be called between BeginOverlay and EndOverlay");
        if (isBuilding)
            CloseOverlay(context.Overlays.Building.back().Id);
    }

    bool IsOverlayOpen(ID id)
    {
        return FindOpen(Internal::GetContext().Overlays, id) != NotOpen;
    }

    bool IsAnyOverlayOpen()
    {
        return !Internal::GetContext().Overlays.Open.empty();
    }

    bool BeginOverlay(ID id, const OverlayOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        OverlayState& state = context.Overlays;
        const size_t index = FindOpen(state, id);
        if (index == NotOpen)
            return false;

        {
            OpenOverlayEntry& entry = state.Open[index];
            entry.LastFrame = context.FrameCount;
            if (!entry.IsStarted)
            {
                entry.IsStarted = true;
                entry.Captures = options.IsModal || options.DismissOnOutsideClick;
                if (entry.Captures)
                {
                    // The overlay takes the keyboard; whatever had focus gets it back afterwards.
                    entry.PreviousFocus = context.Interaction.FocusedID;
                    entry.WasFocusVisible = context.Interaction.IsFocusVisible;
                    context.Interaction.FocusedID = ID();
                }
                SetAnimationValue(HashID("##appear", id), 0.0f);
                SetAnimationValue(HashID("##scrim", id), 0.0f);
            }
        }
        const bool captures = state.Open[index].Captures;

        const bool isTopmost = index + 1 == state.Open.size();
        if (options.DismissOnEscape && isTopmost && !state.IsEscapeConsumed &&
            context.Input.KeyPressed[static_cast<size_t>(Key::Escape)])
        {
            state.IsEscapeConsumed = true;
            CloseFrom(context, index);
            return false;
        }

        DrawList& drawList = context.Draw;
        const Rect display(Vec2(), context.DisplaySize);
        drawList.PushLayer(DrawLayer::Overlay, static_cast<uint32_t>(index));

        if (captures)
        {
            // An invisible button that covers the display, beneath the overlay: it takes every click that
            // misses the overlay, so nothing under it reacts.
            const Internal::InteractionState::LastItemData lastItem = context.Interaction.LastItem;
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            behavior.ActivateOnPress = true;
            const Interaction outside = ButtonBehavior(HashID("##outside", id), display, behavior);
            context.Interaction.LastItem = lastItem;

            const bool isOtherButtonPressed =
                outside.Hovered && (context.Input.MousePressed[static_cast<size_t>(MouseButton::Right)] ||
                                    context.Input.MousePressed[static_cast<size_t>(MouseButton::Middle)]);
            if ((outside.Clicked || isOtherButtonPressed) && !options.IsModal)
            {
                drawList.PopLayer();
                CloseFrom(context, index);
                return false;
            }
        }

        if (options.HasScrim)
        {
            const float scrim = Animate(HashID("##scrim", id), 1.0f, AnimationSpec::Fade(0.2f));
            drawList.PushOpacity(scrim, false);
            drawList.AddRect(display, context.Style.GetColor(StyleColor::Scrim));
            drawList.PopOpacity();
        }

        // The size is last frame's; a new overlay has none yet and stays hidden for its first frame.
        const Vec2 size = state.Open[index].Bounds.GetSize();
        PlacedOverlay placed =
            PlaceOverlay(options, size, context.DisplaySize, options.ShowsArrow ? ArrowLength : 0.0f);
        if (options.Placement == OverlayPlacement::Center || options.Placement == OverlayPlacement::Top)
        {
            const float progress = Animate(HashID("##appear", id), 1.0f, AnimationSpec::Spring(0.35f));
            placed.Origin.Y -= (1.0f - progress) * AppearTravel;
        }

        Internal::ContainerDescription description;
        description.Kind = Internal::ContainerKind::Overlay;
        description.Axis = Axis::Vertical;
        description.Id = id;
        description.Width = options.Width;
        description.Height = options.Height;
        description.Padding = options.Padding;
        description.Spacing = options.Spacing.value_or(context.Style.GetVar(StyleVar::Spacing));
        description.CrossFactor = GetFactor(options.ContentAlignment);
        description.IsFloating = true;
        description.FloatingOrigin = placed.Origin;

        // The overlay does not inherit the opacity of whatever it was opened from.
        drawList.PushOpacity(1.0f, false);
        PushID(id);
        const Internal::LayoutFrame& frame = Internal::BeginContainer(context, description);

        OverlayBuild build;
        build.Id = id;
        build.Captures = captures;
        build.Radius = options.CornerRadius.value_or(context.Style.GetVar(StyleVar::OverlayCornerRadius));
        build.Opacity = drawList.GetOpacity();
        build.ShowsArrow = options.ShowsArrow;
        build.Anchor = options.Anchor;
        build.Placement = placed.Placement;
        build.Shadow = drawList.AddDeferredShadow(context.Style.GetColor(StyleColor::Shadow), ShadowBlur, ShadowOffset);
        build.Background = drawList.AddDeferredSquircle(context.Style.GetColor(StyleColor::OverlayBackground));
        build.Border = drawList.AddDeferredSquircleStroke(context.Style.GetColor(StyleColor::OverlayBorder),
                                                          context.Scale.GetPixelSize());

        // The overlay's own surface takes the pointer from whatever is around and beneath it.
        Internal::UpdateHover(context, id, Rect(frame.Origin, size));

        state.Building.push_back(build);
        return true;
    }

    void EndOverlay()
    {
        Context& context = Internal::GetFrameContext();
        OverlayState& state = context.Overlays;
        const bool isOpen = !state.Building.empty() && context.Layout.Frames.size() > 1 &&
                            context.Layout.Frames.back().Kind == Internal::ContainerKind::Overlay;
        CB_VERIFY(isOpen, "EndOverlay does not match the innermost open Begin call");
        if (!isOpen)
            return;

        const OverlayBuild build = state.Building.back();
        state.Building.pop_back();
        const Rect rect = Internal::EndContainer(context, Internal::ContainerKind::Overlay);

        DrawList& drawList = context.Draw;
        const float smoothing = context.Style.GetVar(StyleVar::CornerSmoothing);
        drawList.ResolveDeferredSquircle(build.Shadow, rect, build.Radius, smoothing);
        drawList.ResolveDeferredSquircle(build.Background, rect, build.Radius, smoothing);
        drawList.ResolveDeferredSquircle(build.Border, rect, build.Radius, smoothing);
        if (build.ShowsArrow && build.Opacity > 0.0f)
        {
            drawList.PushOpacity(build.Opacity, false);
            DrawArrow(context, build, rect);
            drawList.PopOpacity();
        }

        PopID();
        drawList.PopOpacity();
        drawList.PopLayer();

        // The overlay may have closed itself while its content was built.
        const size_t index = FindOpen(state, build.Id);
        if (index != NotOpen)
        {
            // The next frame places the overlay with this size; a change needs that frame to happen.
            if (state.Open[index].Bounds.GetSize() != rect.GetSize())
                context.IsAnimatingThisFrame = true;
            state.Open[index].Bounds = rect;
        }
    }
} // namespace Carbon
