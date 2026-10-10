// Carbon's web app, published on GitHub Pages: a start screen that leads to the Gallery and to the documentation,
// all drawn by Carbon in the browser (WebGL 2, the OpenGL ES backend). The documentation is the repository's Docs
// folder, packed into the build (index.data) and read at startup, so the app always shows the docs of the commit
// it was built from.
//
// The page's address follows the screen ("#gallery/buttons", "#docs/Components/Button.md"), so the browser's Back
// and Forward buttons work and every page can be linked to.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <Carbon/Extensions/NavigationSplitView.h>

#include "DocsReader.h"
#include "ExampleApp.h"
#include "Gallery.h"
#include "StartScreen.h"
#include "WebPlatform.h"

namespace WebApp
{
    namespace
    {
        // Where the build packs the Docs folder in the page's file system.
        constexpr std::string_view DocsRoot = "/Docs";
        constexpr std::string_view GalleryRoute = "gallery";
        constexpr std::string_view DocsRoute = "docs";
        // The ID of the Gallery's navigation split view (Gallery.cpp).
        constexpr std::string_view GalleryNavigationID = "gallery";

        enum class Screen : uint8_t
        {
            Start,
            Gallery,
            Docs
        };

        struct AppState
        {
            Screen Current = Screen::Start;
            Gallery::GalleryState Gallery;
            std::unique_ptr<DocsReader> Docs;

            // What the page's address shows, to notice when the screen changes.
            Screen RouteScreen = Screen::Start;
            Gallery::Page RoutePage = Gallery::Page::Count;
            std::string RouteDocument;
            bool RouteShowsDetail = false;

            // The addresses this page put into the browser's history, and the one shown: going back in the app to
            // the address before goes back in the history instead of adding an entry.
            std::vector<std::string> History;
            size_t HistoryIndex = 0;
        };

        // On a phone the Gallery and the documentation are navigation stacks: whether the page or document is
        // shown rather than the list. In regular width both are.
        bool ShowsDetail(const AppState& state)
        {
            switch (state.Current)
            {
                case Screen::Gallery:
                    return !Carbon::IsCompactWidth() || Carbon::IsNavigationDetailShown(GalleryNavigationID);
                case Screen::Docs:
                    return state.Docs->IsDocumentShown();
                default:
                    return true;
            }
        }

        std::string MakeRoute(const AppState& state)
        {
            switch (state.Current)
            {
                case Screen::Start:
                    return {};
                case Screen::Gallery:
                    if (!ShowsDetail(state))
                        return std::string(GalleryRoute);
                    return std::string(GalleryRoute) + "/" +
                           std::string(Gallery::GetPageKey(state.Gallery.CurrentPage));
                case Screen::Docs:
                    if (!ShowsDetail(state))
                        return std::string(DocsRoute);
                    return std::string(DocsRoute) + "/" + state.Docs->GetCurrentPath();
            }
            return {};
        }

        bool IsRouteCurrent(const AppState& state)
        {
            if (state.Current != state.RouteScreen || ShowsDetail(state) != state.RouteShowsDetail)
                return false;
            if (state.Current == Screen::Gallery)
                return state.Gallery.CurrentPage == state.RoutePage;
            if (state.Current == Screen::Docs)
                return state.Docs->GetCurrentPath() == state.RouteDocument;
            return true;
        }

        void UpdatePageTitle(const AppState& state)
        {
            switch (state.Current)
            {
                case Screen::Start:
                    SetPageTitle("Carbon");
                    break;
                case Screen::Gallery:
                    SetPageTitle("Gallery \xE2\x80\x94 Carbon");
                    break;
                case Screen::Docs:
                    SetPageTitle(std::string(state.Docs->GetCurrentTitle()) + " \xE2\x80\x94 Carbon");
                    break;
            }
        }

        // Remembers the current screen as the one the address shows.
        void MarkRouteCurrent(AppState& state)
        {
            state.RouteScreen = state.Current;
            state.RoutePage = state.Gallery.CurrentPage;
            state.RouteDocument = state.Current == Screen::Docs ? state.Docs->GetCurrentPath() : std::string();
            state.RouteShowsDetail = ShowsDetail(state);
            UpdatePageTitle(state);
        }

        // Shows the screen of an address: "", "gallery[/<page>]" or "docs[/<path>]".
        void ApplyRoute(AppState& state, std::string_view route)
        {
            const size_t slash = route.find('/');
            const std::string_view head = route.substr(0, slash);
            const std::string_view rest =
                slash == std::string_view::npos ? std::string_view() : route.substr(slash + 1);
            // An address without a page or document is the list of them, on a phone.
            if (head == GalleryRoute)
            {
                state.Current = Screen::Gallery;
                const std::optional<Gallery::Page> page = Gallery::FindPage(rest);
                if (page)
                    state.Gallery.CurrentPage = *page;
                Carbon::ShowNavigationDetail(GalleryNavigationID, page.has_value(), false);
            }
            else if (head == DocsRoute && state.Docs->IsLoaded())
            {
                state.Current = Screen::Docs;
                if (!rest.empty())
                    state.Docs->Open(rest);
                state.Docs->ShowDocument(!rest.empty());
            }
            else
                state.Current = Screen::Start;
            MarkRouteCurrent(state);
        }

        void BuildFrame(AppState& state)
        {
            // Back and Forward in the browser.
            if (TakeRouteChange())
            {
                const std::string route = GetRoute();
                if (state.HistoryIndex > 0 && state.History[state.HistoryIndex - 1] == route)
                    state.HistoryIndex--;
                else if (state.HistoryIndex + 1 < state.History.size() &&
                         state.History[state.HistoryIndex + 1] == route)
                    state.HistoryIndex++;
                ApplyRoute(state, route);
            }

            switch (state.Current)
            {
                case Screen::Start:
                    switch (BuildStartScreen(state.Gallery.IsDark, CARBON_WEBAPP_REPOSITORY_URL))
                    {
                        case StartChoice::Gallery:
                            state.Current = Screen::Gallery;
                            break;
                        case StartChoice::Docs:
                            if (state.Docs->IsLoaded())
                                state.Current = Screen::Docs;
                            break;
                        case StartChoice::None:
                            break;
                    }
                    break;
                case Screen::Gallery:
                    if (Gallery::BuildGallery(state.Gallery, {.HasBackButton = true}))
                        state.Current = Screen::Start;
                    break;
                case Screen::Docs:
                    if (state.Docs->Build(state.Gallery.IsDark))
                        state.Current = Screen::Start;
                    break;
            }

            // A new screen, page or document is a new entry in the browser's history; going back to the previous
            // one, with a back button of the app, goes back in the history.
            if (!IsRouteCurrent(state))
            {
                const std::string route = MakeRoute(state);
                if (state.HistoryIndex > 0 && state.History[state.HistoryIndex - 1] == route)
                {
                    state.HistoryIndex--;
                    GoBack();
                }
                else
                {
                    state.History.resize(state.HistoryIndex + 1);
                    state.History.push_back(route);
                    state.HistoryIndex++;
                    PushRoute(route);
                }
                MarkRouteCurrent(state);
            }
        }
    } // namespace
} // namespace WebApp

int main(int argc, char** argv)
{
    Example::App app(argc, argv, "Carbon", 1280, 800);
    if (!app.IsReady())
        return 1;

    WebApp::AppState state;
    state.Gallery.Artwork = Gallery::CreateArtwork(app.GetHost().GetDevice());
    state.Docs = std::make_unique<WebApp::DocsReader>(std::string(WebApp::DocsRoot),
                                                      CARBON_WEBAPP_REPOSITORY_URL "/blob/" CARBON_WEBAPP_SOURCE_REF);
    // The page starts in the appearance the browser prefers.
    state.Gallery.IsDark = WebApp::PrefersDarkAppearance();
    Carbon::SetTheme(state.Gallery.IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());
    WebApp::ApplyRoute(state, WebApp::GetRoute());
    state.History.push_back(WebApp::GetRoute());

    // In a browser Run never returns, so the state stays alive.
    return app.Run([&state] { WebApp::BuildFrame(state); });
}
