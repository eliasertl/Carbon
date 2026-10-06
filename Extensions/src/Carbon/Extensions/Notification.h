#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Where notifications appear in the window.
    enum class NotificationPosition : uint8_t
    {
        TopLeft,
        TopCenter,
        TopRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    /// The kind of message. It chooses the icon and its color when the notification brings no icon or image of
    /// its own.
    enum class NotificationStyle : uint8_t
    {
        /// No icon.
        Plain,
        Info,
        Success,
        Warning,
        Error
    };

    /// Per-call options of PostNotification. All fields are optional.
    struct NotificationOptions
    {
        /// The message below the title, in complete sentences. It wraps.
        std::string_view Body = {};
        NotificationStyle Style = NotificationStyle::Plain;
        /// An icon at the leading edge, such as Carbon::Icons::Bell. Replaces the style's icon.
        std::string_view Icon = {};
        /// Replaces the color of the icon.
        std::optional<Color> IconTint = {};
        /// An image at the leading edge instead of an icon, such as a picture of the sender or a thumbnail. Get the
        /// ID of a texture from the renderer backend's GetTextureID function (WebGPUGetTextureID, ...). The texture
        /// must stay alive while the notification is shown.
        TextureID Image = {};
        /// Draws the image as a circle, as for a person; otherwise it has rounded corners.
        bool ImageIsRound = false;
        /// Up to two buttons below the message. Use short verbs in title case ("Reply", "Mark as Read").
        std::string_view PrimaryAction = {};
        std::string_view SecondaryAction = {};
        /// Seconds the notification stays before it goes away by itself; the time stops while the pointer rests
        /// on it. 0 keeps it until the user dismisses it.
        float Duration = 5.0f;
        /// Replaces the position given to ShowNotifications, for this notification.
        std::optional<NotificationPosition> Position = {};
        /// A value of your own, handed back with the notification's events.
        uint64_t Tag = 0;
    };

    /// What the user did with a notification.
    enum class NotificationEventKind : uint8_t
    {
        /// Clicked the notification itself: show what it is about.
        Clicked,
        PrimaryAction,
        SecondaryAction,
        /// Closed it with its close button.
        Dismissed,
        /// It went away by itself after its duration.
        Expired
    };

    struct NotificationEvent
    {
        ID Notification;
        NotificationEventKind Kind = NotificationEventKind::Clicked;
        uint64_t Tag = 0;
    };

    /// Per-call options of ShowNotifications. All fields are optional.
    struct NotificationCenterOptions
    {
        /// Where notifications appear unless they ask for another position.
        NotificationPosition Position = NotificationPosition::TopRight;
        float Width = 340.0f;
        /// Distance from the edges of the window.
        float Margin = 16.0f;
        /// Distance between notifications in a stack.
        float Spacing = 8.0f;
        /// How many notifications a stack shows at once. Further ones wait until there is room.
        int MaxVisible = 4;
    };

    /// Shows a notification: a banner at an edge of the window that tells about something that happened, and goes
    /// away by itself. The text is copied, so the strings need not outlive the call; very long text is cut. At most
    /// 32 notifications exist at a time; posting more replaces the oldest. Returns the notification's ID.
    ///
    /// It can be called at any time, also outside a frame. The notification appears once ShowNotifications runs.
    ///
    ///     Carbon::PostNotification("Export Finished", { .Body = "Report.pdf was saved to Documents.",
    ///                                                   .Style = Carbon::NotificationStyle::Success,
    ///                                                   .PrimaryAction = "Show" });
    ID PostNotification(std::string_view title, const NotificationOptions& options = {});
    /// Removes a notification, with its closing animation. Does nothing when it is already gone.
    void DismissNotification(ID notification);
    void DismissAllNotifications();
    /// The number of notifications that are shown or waiting.
    int GetNotificationCount();

    /// Draws the notifications, above everything else. Call it once per frame, at the end of the interface.
    /// Returns what the user did with them during this frame; the span is valid until the next call.
    ///
    /// Notifications are not Tab stops and never take the keyboard: they must not interrupt typing.
    std::span<const NotificationEvent> ShowNotifications(const NotificationCenterOptions& options = {});
} // namespace Carbon
