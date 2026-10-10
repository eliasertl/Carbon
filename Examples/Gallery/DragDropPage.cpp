// The gallery's page for drag and drop: the core API, lists and outline views that reorder, and files dropped
// from the system.

#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <vector>

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

        // The songs of the playlist.
        struct Song
        {
            std::string_view Title;
            std::string_view Length;
        };

        const Song Songs[] = {{"Morning Light", "3:42"},  {"Harbour Lights", "4:05"}, {"Paper Planes", "2:58"},
                              {"Slow River", "5:11"},     {"Northern Line", "3:27"},  {"Glass Garden", "4:36"},
                              {"Last Train Home", "3:54"}};

        // The items of the file tree; GalleryState keeps what each folder contains.
        struct FileItem
        {
            std::string_view Name;
            std::string_view Icon;
            bool IsFolder;
        };

        const FileItem FileItems[] = {
            {"Projects", Icons::Folder, true},          {"Roadmap.key", Icons::FileText, false},
            {"Budget.numbers", Icons::FileText, false}, {"Photos", Icons::Folder, true},
            {"Beach.jpg", Icons::FileImage, false},     {"Snow.png", Icons::FileImage, false},
            {"Archive", Icons::Folder, true},           {"Notes.txt", Icons::FileText, false}};

        // The list of items an item is in: the top level, or a folder's contents.
        std::vector<int>* FindContainer(GalleryState& state, int item)
        {
            if (std::find(state.RootItems.begin(), state.RootItems.end(), item) != state.RootItems.end())
                return &state.RootItems;
            for (std::vector<int>& contents : state.FolderItems)
            {
                if (std::find(contents.begin(), contents.end(), item) != contents.end())
                    return &contents;
            }
            return nullptr;
        }

        // Applies a move the outline view reported: the application owns the tree.
        void MoveFileItem(GalleryState& state, const OutlineMove& move)
        {
            const int item = static_cast<int>(move.Item);
            std::vector<int>* from = FindContainer(state, item);
            if (from == nullptr)
                return;
            from->erase(std::find(from->begin(), from->end(), item));
            if (move.Position == OutlineDropPosition::Into)
            {
                std::vector<int>& into =
                    move.Target < 0 ? state.RootItems : state.FolderItems[static_cast<size_t>(move.Target)];
                into.push_back(item);
            }
            else
            {
                std::vector<int>& siblings = *FindContainer(state, static_cast<int>(move.Target));
                auto position = std::find(siblings.begin(), siblings.end(), static_cast<int>(move.Target));
                if (move.Position == OutlineDropPosition::After)
                    ++position;
                siblings.insert(position, item);
            }
            const std::string_view where = move.Position == OutlineDropPosition::Into
                                               ? "into"
                                               : (move.Position == OutlineDropPosition::Before ? "before" : "after");
            const std::string_view target =
                move.Target < 0 ? std::string_view("the top level") : FileItems[move.Target].Name;
            state.LastFileMove = std::format("{} {} {}", FileItems[item].Name, where, target);
        }

        void BuildFileItem(GalleryState& state, int item)
        {
            const FileItem& file = FileItems[item];
            PushID(item);
            const OutlineItem result = BeginOutlineItem(
                file.Name, item == state.SelectedFileItem,
                {.Icon = file.Icon, .HasChildren = file.IsFolder, .IsInitiallyExpanded = item == 0, .Key = item});
            if (result.Picked)
                state.SelectedFileItem = item;
            if (result.IsExpanded)
            {
                for (const int child : state.FolderItems[static_cast<size_t>(item)])
                    BuildFileItem(state, child);
            }
            EndOutlineItem();
            PopID();
        }

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

        BeginSection("Reordering a list",
                     "Drag a song to another place in the playlist: the others make room, and the list reports the "
                     "move for the application to apply.");
        BeginList("playlist", {.Height = 196.0f, .AllowsReordering = true});
        for (const int song : state.Playlist)
        {
            // The ID follows the song, not its place, so that the rows slide when the order changes.
            PushID(song);
            if (ListItem(Songs[song].Title, song == state.SelectedSong,
                         {.Icon = Icons::MusicNote, .Detail = Songs[song].Length}))
                state.SelectedSong = song;
            PopID();
        }
        ApplyListMove(state.Playlist, EndList());
        EndSection();

        BeginSection("Moving items in an outline",
                     "Drag an item onto the upper or lower edge of another to put it before or after it, or onto a "
                     "folder to put it inside. A collapsed folder opens when the drag rests on it.");
        BeginOutlineView("tree", {.Height = 220.0f, .AllowsReordering = true});
        for (const int item : state.RootItems)
            BuildFileItem(state, item);
        const OutlineMove move = EndOutlineView();
        if (move.IsMoved())
            MoveFileItem(state, move);
        Text(std::format("Last move: {}", state.LastFileMove), {.Secondary = true});
        EndSection();
    }
} // namespace Gallery
