#include "Carbon/Layout/Stack.h"

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Layout/LayoutInternal.h"

namespace Carbon
{
    namespace
    {
        float GetFactor(Alignment alignment)
        {
            switch (alignment)
            {
                case Alignment::Leading:
                    return 0.0f;
                case Alignment::Center:
                    return 0.5f;
                case Alignment::Trailing:
                    return 1.0f;
            }
            return 0.0f;
        }

        float GetFactor(VerticalAlignment alignment)
        {
            switch (alignment)
            {
                case VerticalAlignment::Top:
                    return 0.0f;
                case VerticalAlignment::Center:
                    return 0.5f;
                case VerticalAlignment::Bottom:
                    return 1.0f;
            }
            return 0.0f;
        }

        ID GetStackID(Context& context, std::string_view explicitID, const std::source_location& location)
        {
            if (!explicitID.empty())
                return GetID(explicitID);
            return Internal::GetCallSiteID(context, location.file_name(), location.line(), location.column());
        }

        void AddBackground(Context& context, Internal::LayoutFrame& frame, const std::optional<Color>& color,
                           const std::optional<float>& cornerRadius)
        {
            if (!color.has_value())
                return;
            frame.Background = context.Draw.AddDeferredSquircle(*color);
            frame.BackgroundRadius = cornerRadius.value_or(context.Style.GetVar(StyleVar::GroupCornerRadius));
        }
    } // namespace

    void BeginVStack(const VStackOptions& options, const std::source_location& location)
    {
        Context& context = Internal::GetFrameContext();
        Internal::ContainerDescription description;
        description.Kind = Internal::ContainerKind::VStack;
        description.Axis = Axis::Vertical;
        description.Id = GetStackID(context, options.ID, location);
        description.Width = options.Width;
        description.Height = options.Height;
        description.Padding = options.Padding;
        description.Spacing = options.Spacing.value_or(context.Style.GetVar(StyleVar::Spacing));
        description.CrossFactor = GetFactor(options.Alignment);
        description.JustifyFactor = GetFactor(options.Justify);
        Internal::LayoutFrame& frame = Internal::BeginContainer(context, description);
        AddBackground(context, frame, options.Background, options.CornerRadius);
    }

    void EndVStack()
    {
        Internal::EndContainer(Internal::GetFrameContext(), Internal::ContainerKind::VStack);
    }

    void BeginHStack(const HStackOptions& options, const std::source_location& location)
    {
        Context& context = Internal::GetFrameContext();
        Internal::ContainerDescription description;
        description.Kind = Internal::ContainerKind::HStack;
        description.Axis = Axis::Horizontal;
        description.Id = GetStackID(context, options.ID, location);
        description.Width = options.Width;
        description.Height = options.Height;
        description.Padding = options.Padding;
        description.Spacing = options.Spacing.value_or(context.Style.GetVar(StyleVar::Spacing));
        description.CrossFactor = GetFactor(options.Alignment);
        description.JustifyFactor = GetFactor(options.Justify);
        Internal::LayoutFrame& frame = Internal::BeginContainer(context, description);
        AddBackground(context, frame, options.Background, options.CornerRadius);
    }

    void EndHStack()
    {
        Internal::EndContainer(Internal::GetFrameContext(), Internal::ContainerKind::HStack);
    }
} // namespace Carbon
