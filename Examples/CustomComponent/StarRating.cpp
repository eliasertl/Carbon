#include "StarRating.h"

#include <algorithm>
#include <cmath>

namespace Example
{
    using namespace Carbon;

    namespace
    {
        constexpr float StarGap = 4.0f;
        // A star that becomes filled pops: it overshoots its size a little and settles.
        constexpr AnimationSpec PopSpring = AnimationSpec::Spring(0.35f, 0.55f);
    } // namespace

    bool StarRating(std::string_view label, int* rating, const StarRatingOptions& options)
    {
        // 1. Check the arguments. CB_VERIFY reports through the host's callbacks and lets the frame continue.
        CB_VERIFY(rating != nullptr, "StarRating needs a rating to bind to");
        if (rating == nullptr || options.Count <= 0)
            return false;

        // 2. Identity. The ID is derived from the label and the ID stack; everything the component remembers
        //    between frames (focus, animations) hangs on it.
        const ID id = GetID(label);

        PushDisabled(options.Disabled);

        // 3. Layout. Ask the current container for space; the returned rectangle is final for this frame.
        const float count = static_cast<float>(options.Count);
        const Vec2 size(options.StarSize * count + StarGap * (count - 1.0f), options.StarSize);
        const Rect rect = AllocateItem(size);

        // 4. Interaction. ButtonBehavior makes the rectangle hoverable, pressable and a stop for Tab.
        const Interaction interaction = ButtonBehavior(id, rect);

        // The star under the pointer, counted from 1; 0 when the pointer is elsewhere.
        int hovered = 0;
        if (interaction.Hovered)
        {
            const float position = (GetMousePos().X - rect.X) / (options.StarSize + StarGap);
            hovered = std::clamp(static_cast<int>(position) + 1, 1, options.Count);
        }

        const int before = std::clamp(*rating, 0, options.Count);
        int current = before;
        if (interaction.Clicked && hovered > 0)
            current = hovered == before ? 0 : hovered; // clicking the current rating clears it
        if (interaction.Focused)
        {
            // 5. Keyboard. Whatever the mouse can do must work without it.
            if (IsKeyPressed(Key::RightArrow))
                current = std::min(current + 1, options.Count);
            if (IsKeyPressed(Key::LeftArrow))
                current = std::max(current - 1, 0);
        }
        const bool changed = current != before;
        if (changed || current != *rating)
            *rating = current;

        // 6. Draw. Colors come from the three styling layers: the per-call option wins over the pushed style,
        //    which wins over the theme.
        DrawList& drawList = GetDrawList();
        const Color tint = Resolve(options.Tint, StyleColor::Yellow);
        const Color empty = GetStyleColor(StyleColor::TertiaryLabel);
        // While the pointer is over the control it previews the rating a click would give.
        const int shown = hovered > 0 ? hovered : current;
        for (int i = 0; i < options.Count; i++)
        {
            const bool isFilled = i < shown;
            // 7. Animate. Each star has its own animation, identified by an ID derived from the control's.
            //    With Reduce Motion on, Carbon turns the spring into a jump by itself.
            const float fill = Animate(HashID(i, id), isFilled ? 1.0f : 0.0f, PopSpring);
            const float scale = 0.85f + 0.15f * fill;
            const Vec2 center(rect.X + options.StarSize * 0.5f + (options.StarSize + StarGap) * static_cast<float>(i),
                              rect.GetCenter().Y);
            DrawIcon(drawList, center, Icons::Star, options.StarSize * scale, isFilled ? tint : empty,
                     isFilled ? IconVariant::Fill : IconVariant::Regular);
        }

        // 8. Focus ring, drawn only while the user navigates by keyboard.
        DrawFocusRing(id, rect.Expand(2.0f), 6.0f);

        PopDisabled();

        // 9. Tell Carbon about the item, so that Tooltip and IsItemHovered work after the call.
        SetLastItem(id, rect, interaction);
        return changed;
    }
} // namespace Example
