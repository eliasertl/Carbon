#include "Carbon/Extensions/Notification.h"

#include <algorithm>
#include <cstring>

namespace Carbon
{
    namespace
    {
        constexpr int MaxNotifications = 32;
        constexpr int MaxEvents = 32;
        constexpr size_t MaxTitle = 128;
        constexpr size_t MaxBody = 512;
        constexpr size_t MaxIcon = 8;
        constexpr size_t MaxAction = 48;

        constexpr float Padding = 12.0f;
        constexpr float CornerRadius = 14.0f;
        constexpr float VisualSize = 32.0f;
        constexpr float VisualGap = 10.0f;
        constexpr float TitleGap = 2.0f;
        constexpr float ActionHeight = 24.0f;
        constexpr float ActionGap = 8.0f;
        constexpr float ActionPadding = 12.0f;
        constexpr float CloseButtonSize = 18.0f;
        // Notifications slide in from their edge by this much.
        constexpr float SlideDistance = 24.0f;
        constexpr AnimationSpec MoveSpring = AnimationSpec::Spring(0.35f, 0.86f);

        // One notification. The text is copied into fixed buffers: the state must be trivially copyable, and
        // posting must not depend on the caller's strings.
        struct Entry
        {
            ID Id;
            uint64_t Sequence;
            uint64_t Tag;
            char Title[MaxTitle];
            char Body[MaxBody];
            char Icon[MaxIcon];
            char Primary[MaxAction];
            char Secondary[MaxAction];
            Color IconTint;
            TextureID Image;
            float Duration;
            /// Seconds shown with the pointer elsewhere; the notification expires when it reaches Duration.
            float Elapsed;
            NotificationStyle Style;
            NotificationPosition Position;
            bool HasPosition;
            bool HasTint;
            bool ImageIsRound;
            bool HasAppeared;
            bool IsClosing;
            bool IsActive;
        };

        struct NotificationStore
        {
            Entry Entries[MaxNotifications];
            NotificationEvent Events[MaxEvents];
            int EventCount;
            uint64_t NextSequence;
        };

        NotificationStore& GetStore()
        {
            return *GetState<NotificationStore>(HashID("Carbon.Notifications"), StateLifetime::Persistent);
        }

        // Copies `text` into `buffer`, cutting it at a character boundary when it does not fit.
        template <size_t Size>
        void CopyText(char (&buffer)[Size], std::string_view text)
        {
            size_t length = std::min(text.size(), Size - 1);
            // Never end in the middle of a UTF-8 sequence: back off over continuation bytes.
            if (length < text.size())
            {
                while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80)
                    length--;
            }
            std::memcpy(buffer, text.data(), length);
            buffer[length] = '\0';
        }

        Entry* Find(NotificationStore& store, ID id)
        {
            for (Entry& entry : store.Entries)
            {
                if (entry.IsActive && entry.Id == id)
                    return &entry;
            }
            return nullptr;
        }

        void AddEvent(NotificationStore& store, const Entry& entry, NotificationEventKind kind)
        {
            if (store.EventCount < MaxEvents)
                store.Events[store.EventCount++] = NotificationEvent{entry.Id, kind, entry.Tag};
        }

        // The icon and its color from the notification's style, unless it brings its own.
        std::string_view GetIcon(const Entry& entry, Color& tint)
        {
            StyleColor role = StyleColor::Accent;
            std::string_view icon = entry.Icon;
            switch (entry.Style)
            {
                case NotificationStyle::Plain:
                    break;
                case NotificationStyle::Info:
                    icon = icon.empty() ? std::string_view(Icons::Info) : icon;
                    break;
                case NotificationStyle::Success:
                    icon = icon.empty() ? std::string_view(Icons::CheckCircle) : icon;
                    role = StyleColor::Green;
                    break;
                case NotificationStyle::Warning:
                    icon = icon.empty() ? std::string_view(Icons::Warning) : icon;
                    role = StyleColor::Orange;
                    break;
                case NotificationStyle::Error:
                    icon = icon.empty() ? std::string_view(Icons::XCircle) : icon;
                    role = StyleColor::Red;
                    break;
            }
            tint = entry.HasTint ? entry.IconTint : GetStyleColor(role);
            return icon;
        }

        bool IsTop(NotificationPosition position)
        {
            return position == NotificationPosition::TopLeft || position == NotificationPosition::TopCenter ||
                   position == NotificationPosition::TopRight;
        }

        // Text measurements of a notification, for its height and for drawing.
        struct CardLayout
        {
            TextSpec TitleSpec;
            TextSpec BodySpec;
            float TitleHeight = 0.0f;
            float BodyHeight = 0.0f;
            float TextX = 0.0f;
            bool HasVisual = false;
            bool HasActions = false;
            float Height = 0.0f;
        };

        CardLayout MeasureCard(const Entry& entry, float width)
        {
            CardLayout layout;
            Color tint;
            layout.HasVisual = entry.Image.Value != 0 || !GetIcon(entry, tint).empty();
            layout.HasActions = entry.Primary[0] != '\0' || entry.Secondary[0] != '\0';
            layout.TextX = Padding + (layout.HasVisual ? VisualSize + VisualGap : 0.0f);
            const float textWidth = std::max(width - layout.TextX - Padding, 1.0f);

            layout.TitleSpec = GetTextSpec(TextStyle::Body, true);
            layout.TitleSpec.MaxWidth = textWidth;
            layout.TitleSpec.Wraps = true;
            layout.BodySpec = GetTextSpec(TextStyle::Body);
            layout.BodySpec.MaxWidth = textWidth;
            layout.BodySpec.Wraps = true;
            layout.TitleHeight = MeasureText(entry.Title, layout.TitleSpec).Y;
            layout.BodyHeight = entry.Body[0] != '\0' ? MeasureText(entry.Body, layout.BodySpec).Y : 0.0f;

            const float text = layout.TitleHeight + (layout.BodyHeight > 0.0f ? TitleGap + layout.BodyHeight : 0.0f);
            layout.Height = Padding * 2.0f + std::max(text, layout.HasVisual ? VisualSize : 0.0f);
            if (layout.HasActions)
                layout.Height += ActionGap + ActionHeight;
            return layout;
        }

        // A button of a notification. Returns true when clicked.
        bool ActionButton(ID id, const Rect& rect, std::string_view title, bool isPrimary)
        {
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            const Interaction interaction = ButtonBehavior(id, rect, behavior);
            const ControlFeedback feedback = AnimateFeedback(id, interaction.Hovered, interaction.Pressed);
            const Color label = GetStyleColor(StyleColor::Label);
            const Color fill = isPrimary ? GetStyleColor(StyleColor::Accent) : GetStyleColor(StyleColor::ControlFill);
            DrawList& drawList = GetDrawList();
            drawList.AddSquircle(rect, ApplyFeedback(fill, isPrimary ? Color::Black() : label, feedback), 6.0f,
                                 GetStyleVar(StyleVar::CornerSmoothing));
            const TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
            const float width = MeasureText(title, spec).X;
            DrawLabel(drawList, rect, rect.GetCenter().X - width * 0.5f, title, spec,
                      isPrimary ? GetStyleColor(StyleColor::OnAccent) : label);
            return interaction.Clicked;
        }

        void DrawCard(NotificationStore& store, Entry& entry, const CardLayout& layout, const Rect& rect, float opacity)
        {
            DrawList& drawList = GetDrawList();
            const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
            drawList.PushOpacity(opacity, false);

            // The whole card is a button: clicking it opens what the notification is about.
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            const Interaction card = ButtonBehavior(entry.Id, rect, behavior);
            const bool isPointerInside = IsRectHovered(rect.Expand(CloseButtonSize * 0.5f));
            // In touch mode the close button is always there: a finger cannot hover to reveal it.
            const bool showsClose = isPointerInside || IsTouchMode();
            const float hover =
                Animate(HashID("##hover", entry.Id), showsClose ? 1.0f : 0.0f, AnimationSpec::Fade(0.12f));

            drawList.AddShadow(rect, GetStyleColor(StyleColor::Shadow), CornerRadius, 18.0f, Vec2(0.0f, 6.0f),
                               smoothing);
            const Color background = GetStyleColor(StyleColor::OverlayBackground);
            drawList.AddSquircle(rect,
                                 Blend(background, GetStyleColor(StyleColor::Label),
                                       GetStyleVar(StyleVar::HoverAmount) * 0.5f * (card.Pressed ? 2.0f : 0.0f)),
                                 CornerRadius, smoothing);
            drawList.AddSquircleStroke(rect, GetStyleColor(StyleColor::OverlayBorder), CornerRadius,
                                       GetContentScale().GetPixelSize(), smoothing);

            // Image or icon at the leading edge.
            const Rect visual(rect.X + Padding, rect.Y + Padding, VisualSize, VisualSize);
            if (entry.Image.Value != 0)
            {
                drawList.AddImage(entry.Image, visual, Rect(0.0f, 0.0f, 1.0f, 1.0f), Color::White(),
                                  entry.ImageIsRound ? VisualSize * 0.5f : 8.0f, entry.ImageIsRound ? 0.0f : smoothing);
            }
            else
            {
                Color tint;
                const std::string_view icon = GetIcon(entry, tint);
                if (!icon.empty())
                    DrawIcon(drawList, visual.GetCenter(), icon, VisualSize - 4.0f, tint, IconVariant::Fill);
            }

            // Title and message.
            const float textX = rect.X + layout.TextX;
            float y = rect.Y + Padding;
            if (layout.HasVisual && layout.TitleHeight + layout.BodyHeight < VisualSize)
                y += (VisualSize - layout.TitleHeight -
                      (layout.BodyHeight > 0.0f ? TitleGap + layout.BodyHeight : 0.0f)) *
                     0.5f;
            drawList.AddText(Vec2(textX, y), entry.Title, layout.TitleSpec, GetStyleColor(StyleColor::Label));
            if (layout.BodyHeight > 0.0f)
            {
                drawList.AddText(Vec2(textX, y + layout.TitleHeight + TitleGap), entry.Body, layout.BodySpec,
                                 GetStyleColor(StyleColor::SecondaryLabel));
            }

            // Actions, at the trailing edge of the bottom row.
            if (layout.HasActions)
            {
                const TextSpec spec = GetTextSpec(TextStyle::Subheadline, true);
                float right = rect.GetRight() - Padding;
                const float actionY = rect.GetBottom() - Padding - ActionHeight;
                const auto place = [&](std::string_view title)
                {
                    const float width = MeasureText(title, spec).X + ActionPadding * 2.0f;
                    right -= width;
                    const Rect action(right, actionY, width, ActionHeight);
                    right -= ActionGap;
                    return action;
                };
                if (entry.Primary[0] != '\0' &&
                    ActionButton(HashID("##primary", entry.Id), place(entry.Primary), entry.Primary, true))
                {
                    AddEvent(store, entry, NotificationEventKind::PrimaryAction);
                    entry.IsClosing = true;
                }
                if (entry.Secondary[0] != '\0' &&
                    ActionButton(HashID("##secondary", entry.Id), place(entry.Secondary), entry.Secondary, false))
                {
                    AddEvent(store, entry, NotificationEventKind::SecondaryAction);
                    entry.IsClosing = true;
                }
            }

            // The close button appears at the top-left corner while the pointer is on the notification, and
            // always in touch mode.
            if (hover > 0.001f && !entry.IsClosing)
            {
                const Rect close =
                    Rect::FromCenter(rect.GetMin() + Vec2(CloseButtonSize * 0.3f), Vec2(CloseButtonSize));
                ButtonBehaviorOptions closeBehavior;
                closeBehavior.Focusable = false;
                const Interaction closeInteraction = ButtonBehavior(HashID("##close", entry.Id), close, closeBehavior);
                drawList.PushOpacity(hover);
                const Vec2 center = close.GetCenter();
                drawList.AddShadow(close, GetStyleColor(StyleColor::Shadow), CloseButtonSize * 0.5f, 4.0f,
                                   Vec2(0.0f, 1.0f), 0.0f);
                drawList.AddCircle(center, CloseButtonSize * 0.5f, background);
                drawList.AddCircleStroke(center, CloseButtonSize * 0.5f, GetStyleColor(StyleColor::OverlayBorder),
                                         GetContentScale().GetPixelSize());
                const Color mark =
                    GetStyleColor(closeInteraction.Hovered ? StyleColor::Label : StyleColor::SecondaryLabel);
                const float h = 3.5f;
                drawList.AddLine(center + Vec2(-h, -h), center + Vec2(h, h), mark, 1.5f);
                drawList.AddLine(center + Vec2(h, -h), center + Vec2(-h, h), mark, 1.5f);
                drawList.PopOpacity();
                if (closeInteraction.Clicked)
                {
                    AddEvent(store, entry, NotificationEventKind::Dismissed);
                    entry.IsClosing = true;
                }
            }

            if (card.Clicked && !entry.IsClosing)
            {
                AddEvent(store, entry, NotificationEventKind::Clicked);
                entry.IsClosing = true;
            }

            // The time stops while the pointer rests on the notification, so that it can be read.
            if (!isPointerInside && entry.Duration > 0.0f && !entry.IsClosing)
            {
                entry.Elapsed += GetDeltaTime();
                if (entry.Elapsed >= entry.Duration)
                {
                    AddEvent(store, entry, NotificationEventKind::Expired);
                    entry.IsClosing = true;
                }
            }
            drawList.PopOpacity();
        }
    } // namespace

    ID PostNotification(std::string_view title, const NotificationOptions& options)
    {
        NotificationStore& store = GetStore();
        // A free slot, or else the oldest notification makes room.
        Entry* slot = nullptr;
        for (Entry& entry : store.Entries)
        {
            if (!entry.IsActive)
            {
                slot = &entry;
                break;
            }
            if (slot == nullptr || entry.Sequence < slot->Sequence)
                slot = &entry;
        }

        Entry& entry = *slot;
        entry = Entry();
        entry.Sequence = ++store.NextSequence;
        entry.Id = ID{HashCombine(HashSeed, entry.Sequence ^ 0x6E6F7469666963ull)};
        entry.Tag = options.Tag;
        CopyText(entry.Title, title);
        CopyText(entry.Body, options.Body);
        CopyText(entry.Icon, options.Icon);
        CopyText(entry.Primary, options.PrimaryAction);
        CopyText(entry.Secondary, options.SecondaryAction);
        entry.HasTint = options.IconTint.has_value();
        entry.IconTint = options.IconTint.value_or(Color());
        entry.Image = options.Image;
        entry.ImageIsRound = options.ImageIsRound;
        entry.Style = options.Style;
        entry.Duration = std::max(options.Duration, 0.0f);
        entry.HasPosition = options.Position.has_value();
        entry.Position = options.Position.value_or(NotificationPosition::TopRight);
        entry.IsActive = true;
        RequestAnimationFrame();
        return entry.Id;
    }

    void DismissNotification(ID notification)
    {
        if (Entry* entry = Find(GetStore(), notification))
            entry->IsClosing = true;
    }

    void DismissAllNotifications()
    {
        for (Entry& entry : GetStore().Entries)
            entry.IsClosing = entry.IsClosing || entry.IsActive;
    }

    int GetNotificationCount()
    {
        int count = 0;
        for (const Entry& entry : GetStore().Entries)
            count += entry.IsActive ? 1 : 0;
        return count;
    }

    std::span<const NotificationEvent> ShowNotifications(const NotificationCenterOptions& options)
    {
        NotificationStore& store = GetStore();
        store.EventCount = 0;
        const Vec2 display = GetDisplaySize();
        const float width = std::min(options.Width, std::max(display.X - options.Margin * 2.0f, 1.0f));

        DrawList& drawList = GetDrawList();
        drawList.PushLayer(DrawLayer::Overlay, MaxOverlayDepth - 1);

        bool isAnyActive = false;
        constexpr NotificationPosition Positions[] = {
            NotificationPosition::TopLeft,    NotificationPosition::TopCenter,    NotificationPosition::TopRight,
            NotificationPosition::BottomLeft, NotificationPosition::BottomCenter, NotificationPosition::BottomRight};
        for (const NotificationPosition position : Positions)
        {
            // Newest first, nearest to the edge. Order the stack by walking the sequence numbers downwards.
            const bool isTop = IsTop(position);
            float x = options.Margin;
            if (position == NotificationPosition::TopCenter || position == NotificationPosition::BottomCenter)
                x = (display.X - width) * 0.5f;
            else if (position == NotificationPosition::TopRight || position == NotificationPosition::BottomRight)
                x = display.X - options.Margin - width;
            float offset = options.Margin;
            int shown = 0;
            uint64_t below = UINT64_MAX;
            while (shown < options.MaxVisible)
            {
                Entry* next = nullptr;
                for (Entry& entry : store.Entries)
                {
                    const NotificationPosition entryPosition = entry.HasPosition ? entry.Position : options.Position;
                    if (entry.IsActive && entryPosition == position && entry.Sequence < below &&
                        (next == nullptr || entry.Sequence > next->Sequence))
                        next = &entry;
                }
                if (next == nullptr)
                    break;
                Entry& entry = *next;
                below = entry.Sequence;
                shown++;
                isAnyActive = true;

                const CardLayout layout = MeasureCard(entry, width);
                const float y = isTop ? offset : display.Y - offset - layout.Height;
                const Rect target = GetContentScale().Snap(Rect(x, y, width, layout.Height));
                const ID frame = HashID("##frame", entry.Id);
                if (!entry.HasAppeared)
                {
                    // It slides in from its edge: sideways at the corners, vertically in the middle.
                    Vec2 from = Vec2(0.0f, isTop ? -SlideDistance : SlideDistance);
                    if (position == NotificationPosition::TopLeft || position == NotificationPosition::BottomLeft)
                        from = Vec2(-SlideDistance, 0.0f);
                    if (position == NotificationPosition::TopRight || position == NotificationPosition::BottomRight)
                        from = Vec2(SlideDistance, 0.0f);
                    SetAnimationValue(frame, target.Offset(from));
                    SetAnimationValue(HashID("##opacity", entry.Id), 0.0f);
                    entry.HasAppeared = true;
                }
                const Rect rect = Animate(frame, target, MoveSpring);
                const float opacity =
                    Animate(HashID("##opacity", entry.Id), entry.IsClosing ? 0.0f : 1.0f, AnimationSpec::Fade(0.2f));
                DrawCard(store, entry, layout, rect, std::clamp(opacity, 0.0f, 1.0f));

                // A closing notification keeps its place until it has faded, then the others move up.
                if (entry.IsClosing && opacity <= 0.01f)
                    entry.IsActive = false;
                offset += layout.Height + options.Spacing;
            }
        }
        drawList.PopLayer();

        // Notifications that wait or count down need frames even when nothing else moves.
        if (isAnyActive)
            RequestAnimationFrame();
        return std::span<const NotificationEvent>(store.Events, static_cast<size_t>(store.EventCount));
    }
} // namespace Carbon
