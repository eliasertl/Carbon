#include "Carbon/Widgets/Image.h"

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"

namespace Carbon
{
    void Image(TextureID texture, Vec2 size, const ImageOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        const Rect rect = AllocateItem(size);
        context.Draw.AddImage(texture, rect, options.UV, options.Tint, options.CornerRadius,
                              Resolve(options.CornerSmoothing, StyleVar::CornerSmoothing));

        Interaction interaction;
        interaction.Hovered = IsRectHovered(rect);
        SetLastItem(ID(), rect, interaction);
    }
} // namespace Carbon
