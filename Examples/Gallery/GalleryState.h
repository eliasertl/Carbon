#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <Carbon/Extensions/Extensions.h>

namespace Gallery
{
    /// The pages of the gallery, in the order of the sidebar.
    enum class Page
    {
        Typography,
        Icons,
        Buttons,
        Toggles,
        Sliders,
        TextFields,
        Images,
        Layout,

        Selection,
        Dates,
        Menus,
        Toolbars,
        Dialogs,
        Notifications,
        Progress,
        Lists,
        Hierarchies,
        DragAndDrop,
        Navigation,
        Charts,
#if defined(CARBON_GALLERY_HAVE_REFLECTION)
        Reflection,
#endif

        Count
    };

    /// The state the interface edits. In an immediate-mode UI the application owns all of it.
    struct GalleryState
    {
        Page CurrentPage = Page::Buttons;
        /// From --show: something a page opens once, so that screenshots can capture menus and dialogs.
        std::string Show;
        bool IsDark = false;
        bool ReduceMotion = false;
        /// The system has an emoji font, which the example added as a fallback: the typography page shows emoji.
        bool HasEmojiFont = false;

        // Core pages.
        bool WiFi = true;
        bool Bluetooth = false;
        bool AirplaneMode = false;
        bool ShowHidden = false;
        bool AutoSave = true;
        bool SyncAll = false;
        float Volume = 0.62f;
        float Brightness = 0.8f;
        float Steps = 4.0f;
        int Rating = 4;
        double Frequency = 440.0;
        float Equalizer[5] = {4.0f, 7.0f, 2.0f, -3.0f, 5.0f};
        std::string Name = "Ada Lovelace";
        std::string Email;
        std::string Password = "hunter2";
        std::string Search;
        std::string Notes =
            "Carbon is an immediate-mode UI framework with a macOS look. This text area wraps its lines at the edge "
            "and scrolls when the text is longer than the area.\n\nSelect text with the mouse or with Shift and the "
            "arrow keys, and copy, cut and paste it.";
        std::string Outline = "Groceries\n\tApples\n\tBread";
        int Clicks = 0;
        /// A texture of the host's, made by its graphics device.
        Carbon::TextureID Artwork;
        std::string FormName = "Ada Lovelace";
        bool FormUpdates = true;
        bool FormBeta = false;
        float FormVolume = 0.4f;

        // Selection controls.
        int ViewMode = 1;
        std::vector<std::string> Recipients = {"Ada Lovelace", "Grace Hopper", "Alan Kay"};
        std::vector<std::string> Tags = {"design", "macOS", "immediate mode", "WebGPU"};
        std::string TokenAction = "None yet";
        // Dates. Today is the computer's date, except in screenshots, which show a fixed day.
        std::optional<Carbon::DateTime> Today;
        Carbon::DateTime CalendarDate = {.Year = 2026, .Month = 10, .Day = 5, .Hour = 14, .Minute = 30};
        Carbon::DateTime SundayDate = {.Year = 2026, .Month = 10, .Day = 5};
        Carbon::DateTime LimitedDate = {.Year = 2026, .Month = 10, .Day = 12};
        Carbon::DateTime Appointment = {.Year = 2026, .Month = 10, .Day = 5, .Hour = 14, .Minute = 30};
        Carbon::DateTime Deadline = {.Year = 2026, .Month = 10, .Day = 20, .Hour = 17, .Minute = 0};
        Carbon::DateTime Alarm = {.Year = 2026, .Month = 10, .Day = 5, .Hour = 7, .Minute = 15};
        // Toolbars.
        bool ToolbarShowsSidebar = true;
        int ToolbarView = 1;
        std::string ToolbarQuery;
        std::string ToolbarAction = "None yet";
        int Appearance = 2;
        int IconSize = 1;
        int StartWith = -1;
        int Justification = 0;
        int SortOrder = 1;
        int Quality = 2;
        int Copies = 2;
        double FontSize = 13.0;
        int Quantity = 3;
        double Price = 19.99;
        float Opacity = 80.0f;
        double CornerRadius = 12.0;
        int Angle = 45;
        Carbon::Color Tint = Carbon::Color::FromHex(0x34C759);
        Carbon::Color Shadow = Carbon::Color::FromHex(0x5856D6, 0.6f);
        std::string Query;
        std::string LastAction = "None yet";

        // Combo boxes.
        std::string FontName = "Public Sans";
        std::string City;

        // Menu bar.
        bool ShowsToolbar = true;
        bool ShowsStatusBar = false;
        std::string MenuBarAction = "None yet";

        // Outline and column views.
        const void* OutlineSelection = nullptr;
        std::string OutlineAction = "None yet";
        std::vector<int> ColumnPath = {0, 0, 0, 1};

        // Notifications.
        int NotificationPosition = 2;
        int NotificationStyle = 3;
        bool NotificationHasImage = false;
        bool NotificationHasActions = true;
        bool NotificationIsSticky = false;
        int NotificationsPosted = 0;
        std::string NotificationEvent = "None yet";

        // Menus and popovers.
        bool ShowRuler = true;
        bool ShowGrid = false;
        bool Notifications = true;
        float PopoverVolume = 0.4f;

        // Alerts and sheets.
        std::string AlertAnswer = "None yet";
        std::string ExportName = "Quarterly Report";
        int ExportFormat = 0;
        bool ExportTransparent = false;

        // Progress.
        float Progress = 0.45f;

        // Lists and tables.
        int SelectedFile = 2;
        int SelectedTask = 1;
        int SelectedStation = 0;
        Carbon::TableSort RainfallSort = {.Column = 0};
        /// The rows of the rainfall table, in the order of RainfallSort.
        std::vector<int> RainfallRows;
        /// The arrangement of the rainfall table's 42 columns, which the application could save.
        std::array<float, 42> RainfallWidths = {};
        std::array<int, 42> RainfallOrder = {};
        int RainfallArrangementChanges = 0;
        bool TaskDone[6] = {true, false, false, true, false, false};

        // Drag and drop: the box each tag is in.
        std::array<int, 6> TagBoxes = {0, 0, 1, 0, 1, 0};

        // Tabs and split views.
        int Tab = 0;
        int Mailbox = 0;

        // Charts.
        bool ShowsPoints = false;
    };

    /// True once for the --show value `name`: the page then opens what it names.
    inline bool TakeShow(GalleryState& state, std::string_view name)
    {
        if (state.Show != name)
            return false;
        state.Show.clear();
        return true;
    }
} // namespace Gallery
