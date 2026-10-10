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
        /// In compact width (Carbon/Input/Adaptive.h), side-by-side panes become a navigation stack: the first
        /// pane fills the area, choosing an item in it slides the second one in, and a back button or a swipe from
        /// the leading edge returns. Vertical split views never collapse.
        bool CollapsesInCompactWidth = true;
        /// In compact width, the title of the first pane's navigation bar and of the back button (empty: no bar,
        /// and "Back"), and the title of the second pane's navigation bar.
        std::string_view Title = {};
        std::string_view DetailTitle = {};
    };

    /// Two panes separated by a divider that the user can drag. Each pane lays its content out like a VStack.
    ///
    ///     Carbon::BeginSplitView("main");
    ///         BuildList();                   // first pane
    ///     Carbon::SplitViewDivider();
    ///         BuildDetail();                 // second pane
    ///     Carbon::EndSplitView();
    ///
    /// The divider is a stop for Tab; with focus, the arrow keys move it. Split views can be nested. In compact width
    /// a horizontal split view is a navigation stack instead (see CollapsesInCompactWidth and ShowNavigationDetail
    /// in NavigationSplitView.h).
    void BeginSplitView(std::string_view id, const SplitViewOptions& options = {});
    /// Ends the first pane, adds the divider and starts the second pane.
    void SplitViewDivider();
    void EndSplitView();
} // namespace Carbon
