#include "Carbon/Widgets/Image.h"

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Interaction/Gesture.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"

namespace Carbon
{
    void Image(TextureID texture, Vec2 size, const ImageOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        const Rect rect = AllocateItem(size);
        const float smoothing = Resolve(options.CornerSmoothing, StyleVar::CornerSmoothing);
        if (!options.Zoomable)
        {
            context.Draw.AddImage(texture, rect, options.UV, options.Tint, options.CornerRadius, smoothing);
        }
        else
        {
            // The zoomed image is drawn where it covers the frame, with the part of the texture that shows there,
            // so that the frame keeps its rounded corners.
            const ID id = options.ID.empty() ? HashID("##image", ID{texture.Value}) : GetID(options.ID);
            const Zoom zoom = ZoomBehavior(id, rect, {.MaxScale = options.MaxZoom});
            const Rect zoomed = zoom.GetZoomedRect(rect);
            const Rect shown = rect.GetIntersection(zoomed);
            if (shown.Width > 0.0f && shown.Height > 0.0f && zoomed.Width > 0.0f && zoomed.Height > 0.0f)
            {
                const Rect& uv = options.UV;
                const Vec2 from = (shown.GetMin() - zoomed.GetMin()) / zoomed.GetSize();
                const Vec2 to = (shown.GetMax() - zoomed.GetMin()) / zoomed.GetSize();
                const Rect part(uv.X + uv.Width * from.X, uv.Y + uv.Height * from.Y, uv.Width * (to.X - from.X),
                                uv.Height * (to.Y - from.Y));
                context.Draw.AddImage(texture, shown, part, options.Tint, options.CornerRadius, smoothing);
            }
        }

        Interaction interaction;
        interaction.Hovered = IsRectHovered(rect);
        SetLastItem(ID(), rect, interaction);
    }
} // namespace Carbon
