#include "Carbon/Widgets/Tooltip.h"

#include <algorithm>
#include <cstring>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Hash.h"
#include "Carbon/Core/State.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Text/TextStyle.h"

namespace Carbon
{
    namespace
    {
        struct TooltipState
        {
            /// Seconds the pointer has rested on the item.
            float HoverTime;
            /// Where the tooltip was placed when it appeared; it stays there while the pointer moves.
            Vec2 Anchor;
            bool IsPlaced;
        };

        constexpr Vec2 Padding(7.0f, 4.0f);
        // Below and slightly right of the pointer, clear of the cursor image.
        constexpr Vec2 PointerOffset(2.0f, 22.0f);
        constexpr float ScreenMargin = 4.0f;
    } // namespace

    void Tooltip(std::string_view text)
    {
        Context& context = Internal::GetFrameContext();
        const Internal::InteractionState::LastItemData& item = context.Interaction.LastItem;

        // Items without an ID (text, images) are told apart by where they are.
        uint64_t key = item.Id.Value;
        if (!item.Id.IsValid())
        {
            uint32_t bits[2] = {0, 0};
            std::memcpy(bits, &item.Bounds.X, sizeof(bits));
            key = HashCombine(HashSeed, (static_cast<uint64_t>(bits[0]) << 32) | bits[1]);
        }
        const ID id = HashID("##tooltip", ID{key == 0 ? 1 : key});
        TooltipState& state = *GetState<TooltipState>(id);

        bool isAnyButtonDown = false;
        for (const bool down : context.Input.MouseDown)
            isAnyButtonDown = isAnyButtonDown || down;

        if (item.Hovered && !isAnyButtonDown)
        {
            state.HoverTime += context.DeltaTime;
            if (state.HoverTime < TooltipDelay)
                context.IsAnimatingThisFrame = true; // keep frames coming while the delay runs
        }
        else
        {
            state.HoverTime = 0.0f;
            state.IsPlaced = false;
        }

        const bool isVisible = state.HoverTime >= TooltipDelay;
        const float opacity = Animate(HashID("##opacity", id), isVisible ? 1.0f : 0.0f, AnimationSpec::Fade(0.12f));
        if (opacity <= 0.001f || text.empty())
            return;

        const TextSpec spec = GetTextSpec(TextStyle::Subheadline);
        const Vec2 size = MeasureText(text, spec) + Padding * 2.0f;
        if (isVisible && !state.IsPlaced)
        {
            state.Anchor = context.Input.MousePos + PointerOffset;
            state.IsPlaced = true;
        }

        // Keep the tooltip on the display.
        Vec2 origin = state.Anchor;
        origin.X =
            std::clamp(origin.X, ScreenMargin, std::max(ScreenMargin, context.DisplaySize.X - size.X - ScreenMargin));
        origin.Y =
            std::clamp(origin.Y, ScreenMargin, std::max(ScreenMargin, context.DisplaySize.Y - size.Y - ScreenMargin));
        const Rect rect = context.Scale.Snap(Rect(origin, size));

        DrawList& drawList = context.Draw;
        const float radius = 5.0f;
        drawList.PushLayer(DrawLayer::Tooltip);
        drawList.PushOpacity(opacity);
        drawList.AddShadow(rect, context.Style.GetColor(StyleColor::Shadow), radius, 8.0f, Vec2(0.0f, 2.0f));
        drawList.AddSquircle(rect, context.Style.GetColor(StyleColor::OverlayBackground), radius);
        drawList.AddSquircleStroke(rect, context.Style.GetColor(StyleColor::OverlayBorder), radius,
                                   context.Scale.GetPixelSize());
        drawList.AddText(rect.GetMin() + Padding, text, spec, context.Style.GetColor(StyleColor::Label));
        drawList.PopOpacity();
        drawList.PopLayer();
    }
} // namespace Carbon
