#pragma once

#include <string_view>

#include "Carbon/Extension.h"

namespace Carbon
{
    /// Per-call options of BeginNavigationSplitView. All fields are optional.
    struct NavigationSplitViewOptions
    {
        /// In compact width, the title of the navigation bar above the sidebar, and of the detail's back button.
        /// Empty: the sidebar has no bar, and the back button reads "Back".
        std::string_view Title = {};
        /// In compact width, the title in the middle of the detail's navigation bar: usually the name of what the
        /// sidebar selected.
        std::string_view DetailTitle = {};
        /// In compact width, a back button with this title at the leading end of the sidebar's navigation bar, for
        /// a split view the application opened from somewhere else (a start screen). EndNavigationSplitView
        /// reports that it was pressed. In regular width, show a button of your own.
        std::string_view RootBackTitle = {};
        Size Width = Size::Fill();
        Size Height = Size::Fill();
    };

    /// A sidebar and the content it navigates, as in the HIG's split view for iPad and iPhone. In regular width the
    /// sidebar stands next to the content, exactly like a Sidebar followed by a VStack in an HStack. In compact
    /// width (Carbon/Input/Adaptive.h) they become a navigation stack: the sidebar fills the area as a list,
    /// choosing an item in it slides the content in, and the back button of the content's navigation bar or a
    /// swipe from the leading edge returns to the list.
    ///
    ///     Carbon::BeginNavigationSplitView("main", { .Title = "Mail", .DetailTitle = mailbox.Name });
    ///     Carbon::BeginSidebar("mailboxes");
    ///     ...                                       // SidebarItems; picking one shows the detail
    ///     Carbon::EndSidebar();
    ///     Carbon::NavigationSplitViewDetail();
    ///     ...                                       // the content, laid out like in a VStack that fills the rest
    ///     Carbon::EndNavigationSplitView();
    ///
    /// SplitView collapses the same way (SplitViewOptions::CollapsesInCompactWidth).
    void BeginNavigationSplitView(std::string_view id, const NavigationSplitViewOptions& options = {});
    /// Ends the sidebar and starts the content.
    void NavigationSplitViewDetail();
    /// Returns true in the frame the back button of RootBackTitle was pressed.
    bool EndNavigationSplitView();

    /// Shows the content (`true`) or the sidebar (`false`) of a collapsed navigation split view or split view `id`,
    /// for example when the application navigates from code or from an address; `animated` slides it. Picking an
    /// item in the sidebar and going back do this by themselves. No effect is visible in regular width, where both
    /// are shown; the choice is kept for when the width becomes compact.
    void ShowNavigationDetail(std::string_view id, bool isShown, bool animated = true);
    /// True when the content of `id` is shown in compact width (the sidebar is pushed aside).
    bool IsNavigationDetailShown(std::string_view id);
} // namespace Carbon
