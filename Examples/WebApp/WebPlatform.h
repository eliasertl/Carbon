#pragma once

#include <string>
#include <string_view>

// What the web app asks of the browser page around its canvas: the address, the history, new tabs and the
// system appearance.
namespace WebApp
{
    /// The page's route: its address after the '#', such as "docs/Building.md".
    std::string GetRoute();
    /// Makes `route` the page's address as a new history entry, so that the browser's Back button returns to the
    /// previous one.
    void PushRoute(std::string_view route);
    /// True once after the user went back or forward in the browser's history; GetRoute then has the new route.
    bool TakeRouteChange();
    /// Goes back one entry in the browser's history, as the browser's Back button does.
    void GoBack();

    /// Sets the title of the browser tab.
    void SetPageTitle(std::string_view title);
    /// Opens an address in a new browser tab.
    void OpenUrl(std::string_view url);
    /// The system or browser prefers the dark appearance.
    bool PrefersDarkAppearance();
} // namespace WebApp
