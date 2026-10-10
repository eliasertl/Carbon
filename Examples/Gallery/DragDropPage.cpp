// The gallery's page for drag and drop: the core API, lists and outline views that reorder, and files dropped
// from the system.

#include <array>
#include <format>
#include <string_view>

#include "Pages.h"

namespace Gallery
{
    using namespace Carbon;

    namespace
    {
        // Tags that are dragged between two boxes.
        struct TagSample
        {
            std::string_view Name;
            StyleColor Tint;
        };

        const TagSample Tags[] = {{"Urgent", StyleColor::Red},    {"Design", StyleColor::Purple},
                                  {"Review", StyleColor::Orange}, {"Docs", StyleColor::Blue},
                                  {"Shipped", StyleColor::Green}, {"Idea", StyleColor::Yellow}};
        const std::string_view Boxes[] = {"To Do", "Done"};

        // A tag drawn as a tinted capsule.
        void TagChip(const TagSample& tag)
        {
            const Color tint = GetStyleColor(tag.Tint);
            BeginHStack({.Spacing = 5.0f,
                         .Padding = EdgeInsets(10.0f, 4.0f),
                         .Background = tint.WithOpacity(0.16f),
                         .CornerRadius = 12.0f,
                         .ID = tag.Name});
            Icon(Icons::Tag, {.Size = 14.0f, .Color = tint, .Variant = IconVariant::Fill});
            Text(tag.Name, {.Color = tint, .Weight = FontWeight::Semibold});
            EndHStack();
        }
    } // namespace

    void DragDropPage(GalleryState& state)
    {
        BeginSection("Drag and drop",
                     "Drag a tag to the other box. Any item can be a drag source with a typed payload, and any "
                     "rectangle a drop target for the types it accepts. Escape cancels a drag.");
        BeginHStack({.Spacing = 12.0f, .Alignment = VerticalAlignment::Top, .Width = Size::Fill()});
        for (int box = 0; box < static_cast<int>(std::size(Boxes)); box++)
        {
            BeginVStack({.Spacing = 8.0f,
                         .Padding = EdgeInsets(12.0f),
                         .Width = Size::Fill(),
                         .Height = 168.0f,
                         .Background = GetStyleColor(StyleColor::ControlBackground),
                         .CornerRadius = 8.0f,
                         .ID = Boxes[box]});
            Text(Boxes[box], {.Style = TextStyle::Headline});
            for (int tag = 0; tag < static_cast<int>(std::size(Tags)); tag++)
            {
                if (state.TagBoxes[static_cast<size_t>(tag)] != box)
                    continue;
                PushID(tag);
                TagChip(Tags[tag]);
                if (BeginDragSource(GetID("tag"), GetLastItemRect()))
                {
                    SetDragPayload("Tag", tag);
                    TagChip(Tags[tag]);
                    EndDragSource();
                }
                PopID();
            }
            EndVStack();
            GetDrawList().AddSquircleStroke(GetLastItemRect(), GetStyleColor(StyleColor::ControlBorder), 8.0f,
                                            GetContentScale().GetPixelSize());
            // The box takes tags dropped onto it.
            const Drop drop = AcceptDrop(GetID(Boxes[box]), GetLastItemRect(), "Tag", {.CornerRadius = 8.0f});
            if (drop.IsDelivered)
                state.TagBoxes[static_cast<size_t>(drop.Payload.As<int>())] = box;
        }
        EndHStack();
        EndSection();
    }
} // namespace Gallery
