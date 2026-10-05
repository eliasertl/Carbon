#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// One component of a path: a folder, a disk, a document.
    struct PathControlItem
    {
        std::string_view Label = {};
        /// An icon in front of the label, e.g. Icons::Folder. A component whose name is hidden shows only this.
        std::string_view Icon = {};
    };

    enum class PathControlStyle : uint8_t
    {
        /// Every component in a row, separated by chevrons.
        Standard,
        /// A button showing the last component; the whole path opens as a menu.
        PopUp
    };

    /// Per-call options of PathControl. All fields are optional.
    struct PathControlOptions
    {
        PathControlStyle Style = PathControlStyle::Standard;
        /// Fit shows every name. With a fixed or Fill width that is too narrow, the standard style hides the
        /// names between the first and the last component.
        Size Width = Size::Fit();
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// Shows a path, from its root (the first item) to the selected item (the last). The application owns the
    /// path; the control reports which component the user clicked or chose with the keyboard. Returns that
    /// component's index on the frame it was activated, otherwise -1.
    ///
    /// The label identifies the control and is not drawn.
    int PathControl(std::string_view label, std::span<const PathControlItem> path,
                    const PathControlOptions& options = {});
} // namespace Carbon
