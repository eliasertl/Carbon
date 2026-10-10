#pragma once

#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon::Internal
{
    /// The compact-width layout shared by SplitView and NavigationSplitView: a navigation stack of two panes. The
    /// root (the first pane, usually a sidebar or a list) fills the area; choosing an item in it pushes the detail
    /// (the second pane), which slides in from the trailing edge with a navigation bar whose back button, or a
    /// swipe from the leading edge, goes back. Both panes are built every frame; the one that is off screen is
    /// clipped away.
    ///
    ///     BeginCollapsedNavigation(id, "Library", width, height);   // the root pane begins
    ///     ...
    ///     CollapsedNavigationDetail("Songs");                        // the detail pane begins
    ///     ...
    ///     EndCollapsedNavigation();
    void BeginCollapsedNavigation(ID id, std::string_view rootTitle, Size width, Size height);
    void CollapsedNavigationDetail(std::string_view detailTitle);
    void EndCollapsedNavigation();

    /// True while the root pane of a collapsed navigation is being built: components that fill the width there
    /// (a sidebar becomes a full-width list with disclosure chevrons).
    bool IsInCollapsedNavigationRoot();
    /// Called by the rows of selection lists when the user picks one: in the root pane of a collapsed navigation
    /// that pushes the detail.
    void NotifyNavigationChoice();

    /// The state behind PushNavigationDetail and friends, for the navigation stack with this ID.
    void SetNavigationDetailShown(ID id, bool isShown, bool animated);
    bool IsNavigationDetailShown(ID id);
} // namespace Carbon::Internal
