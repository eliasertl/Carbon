#pragma once

#include <cstdint>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Which button of an alert was chosen.
    enum class AlertResult : uint8_t
    {
        /// The alert is closed, or still waiting for an answer.
        None,
        Primary,
        Secondary
    };

    /// Per-call options of Alert. All fields are optional.
    struct AlertOptions
    {
        /// Informative text below the title.
        std::string_view Message = {};
        /// A large icon above the title, such as Carbon::Icons::Warning.
        std::string_view Icon = {};
        /// The button that carries out the alert's action.
        std::string_view PrimaryLabel = "OK";
        /// The button that backs out, usually "Cancel". Empty leaves it out.
        std::string_view SecondaryLabel = {};
        /// The primary action destroys data. Its button is drawn in the destructive color and, as the HIG asks,
        /// is not the default button: Enter chooses the secondary button instead.
        bool IsDestructive = false;
        float Width = 260.0f;
    };

    /// Opens the alert `id`. Call it at the same ID scope as Alert.
    void OpenAlert(std::string_view id);

    /// An alert interrupts with critical information and waits for an answer. It is modal: nothing else reacts
    /// until a button is chosen. Call it every frame; it returns the chosen button once, on the frame the alert
    /// closes. Enter chooses the default button, Escape the secondary one.
    ///
    ///     if (Carbon::Button("Delete"))
    ///         Carbon::OpenAlert("confirm");
    ///     if (Carbon::Alert("confirm", "Delete the file?", { .Message = "You cannot undo this.",
    ///             .PrimaryLabel = "Delete", .SecondaryLabel = "Cancel", .IsDestructive = true })
    ///         == Carbon::AlertResult::Primary)
    ///         DeleteFile();
    AlertResult Alert(std::string_view id, std::string_view title, const AlertOptions& options = {});
} // namespace Carbon
