#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// What the items of a toolbar show.
    enum class ToolbarDisplayMode : uint8_t
    {
        /// The icon with the label below it.
        IconAndLabel,
        /// Only the icon; the label becomes the item's tooltip.
        IconOnly,
        /// Only the label.
        LabelOnly
    };

    /// Per-call options of BeginToolbar. All fields are optional.
    struct ToolbarOptions
    {
        /// Fill spans the window. A toolbar narrower than its items moves the items that do not fit into an
        /// overflow menu.
        Size Width = Size::Fill();
        /// 52 points with labels below the icons, 38 otherwise, when not set.
        std::optional<float> Height = {};
        /// Defaults to the theme's SecondaryBackground. Pass Color::Transparent() for a toolbar that sits on
        /// something else, such as a title bar.
        std::optional<Color> Background = {};
        /// A hairline along the bottom edge.
        bool HasSeparator = true;
        ToolbarDisplayMode DisplayMode = ToolbarDisplayMode::IconAndLabel;
    };

    /// Per-call options of ToolbarItem. All fields are optional.
    struct ToolbarItemOptions
    {
        /// An icon such as Icons::Plus. Without one, the item shows its label in every display mode.
        std::string_view Icon = {};
        /// Shows the item as turned on, for items that toggle something (a sidebar, an inspector).
        bool IsSelected = false;
        bool Disabled = false;
    };

    /// Per-call options of BeginToolbarControl. All fields are optional.
    struct ToolbarControlOptions
    {
        /// The icon of the control's entry in the overflow menu.
        std::string_view Icon = {};
    };

    /// A toolbar: a bar of frequently used commands and controls below the title bar.
    ///
    ///     Carbon::BeginToolbar("Main");
    ///     if (Carbon::ToolbarItem("Back", { .Icon = Carbon::Icons::CaretLeft }))
    ///         GoBack();
    ///     Carbon::ToolbarFlexibleSpace();
    ///     if (Carbon::BeginToolbarControl("Search", { .Icon = Carbon::Icons::MagnifyingGlass }))
    ///     {
    ///         Carbon::SearchField("Search", &query);
    ///         Carbon::EndToolbarControl();
    ///     }
    ///     Carbon::EndToolbar();
    ///
    /// When the toolbar is too narrow for its items, those at its trailing end move into an overflow menu behind
    /// a chevron button. Choosing an item there activates it: its ToolbarItem returns true in the next frame. A
    /// control chosen there opens in a popover below the chevron.
    void BeginToolbar(std::string_view id, const ToolbarOptions& options = {});
    void EndToolbar();

    /// A command in the toolbar: an icon, a label or both, depending on the display mode. Returns true on the
    /// frame it was activated.
    bool ToolbarItem(std::string_view label, const ToolbarItemOptions& options = {});

    /// Takes the free space of the toolbar, pushing the following items to its trailing end.
    void ToolbarFlexibleSpace();
    /// A fixed gap between groups of items.
    void ToolbarSpace();
    /// A thin vertical line between groups of items.
    void ToolbarSeparator();

    /// Hosts any control in the toolbar: a search field, a segmented control, a pop-up or pull-down button. The
    /// label names the control's entry in the overflow menu. Returns true when the control is to be built; then
    /// add it and call EndToolbarControl.
    bool BeginToolbarControl(std::string_view label, const ToolbarControlOptions& options = {});
    void EndToolbarControl();
} // namespace Carbon
