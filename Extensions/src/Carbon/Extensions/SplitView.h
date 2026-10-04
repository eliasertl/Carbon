#pragma once

#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginSplitView. All fields are optional.
    struct SplitViewOptions
    {
        /// Horizontal puts the panes side by side, Vertical one above the other.
        Carbon::Axis Axis = Carbon::Axis::Horizontal;
        /// Length of the first pane before the user moves the divider.
        float InitialSize = 240.0f;
        /// Neither pane gets shorter than this.
        float MinSize = 120.0f;
        float MinSecondSize = 120.0f;
        Size Width = Size::Fill();
        Size Height = Size::Fill();
    };

    /// Two panes separated by a divider that the user can drag. Each pane lays its content out like a VStack.
    ///
    ///     Carbon::BeginSplitView("main");
    ///         BuildList();                   // first pane
    ///     Carbon::SplitViewDivider();
    ///         BuildDetail();                 // second pane
    ///     Carbon::EndSplitView();
    ///
    /// The divider is a stop for Tab; with focus, the arrow keys move it. Split views can be nested.
    void BeginSplitView(std::string_view id, const SplitViewOptions& options = {});
    /// Ends the first pane, adds the divider and starts the second pane.
    void SplitViewDivider();
    void EndSplitView();
} // namespace Carbon
