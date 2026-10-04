#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginSheet. All fields are optional.
    struct SheetOptions
    {
        Size Width = Size::Fixed(420.0f);
        Size Height = Size::Fit();
        EdgeInsets Padding = EdgeInsets(20.0f);
        /// Distance between items. Defaults to the theme's Spacing.
        std::optional<float> Spacing = {};
        /// Escape closes the sheet. Turn it off when closing must go through the sheet's own buttons.
        bool DismissOnEscape = true;
    };

    /// Opens the sheet `id`. Call it at the same ID scope as BeginSheet.
    void OpenSheet(std::string_view id);
    /// Closes the sheet `id` and anything opened from it. Like OpenSheet it must be called at the ID scope of
    /// BeginSheet, so not from inside the sheet: there, use CloseCurrentSheet.
    void CloseSheet(std::string_view id);
    bool IsSheetOpen(std::string_view id);

    /// A sheet is a modal view for a self-contained task, such as settings for an export. It appears centered
    /// over the dimmed interface, which does not react until the sheet is closed. Returns true while it is open;
    /// then add its content, laid out like in a VStack, and call EndSheet. Give it buttons that close it.
    ///
    ///     if (Carbon::BeginSheet("export"))
    ///     {
    ///         Carbon::Text("Export", { .Style = Carbon::TextStyle::Title2 });
    ///         if (Carbon::Button("Done", { .Role = Carbon::ButtonRole::Prominent, .IsDefault = true }))
    ///             Carbon::CloseCurrentSheet();
    ///         Carbon::EndSheet();
    ///     }
    bool BeginSheet(std::string_view id, const SheetOptions& options = {});
    void EndSheet();
    /// Closes the sheet whose content is being built. Unlike CloseSheet it works at any ID scope.
    void CloseCurrentSheet();
} // namespace Carbon
