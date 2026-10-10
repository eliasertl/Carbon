// Carbon Gallery: every component, in both themes. This is Carbon's visual benchmark; it should look like a
// macOS app without the window chrome.
//
//   <Backend>Gallery [--theme light|dark] [--scale <factor>] [--size <width>x<height>] [--page <name>]
//                    [--screenshot <file.png>] [--show menu|popover|alert|sheet] [--pointer <x>x<y>] [--click <x>x<y>]

#include <cstdio>
#include <optional>

#include "ExampleApp.h"
#include "Gallery.h"
#include "Pages.h"

int main(int argc, char** argv)
{
    Example::App app(argc, argv, "Carbon Gallery", 1040, 740);
    if (!app.IsReady())
        return 1;

    Gallery::GalleryState state;
    state.IsDark = app.GetArguments().IsDark;
    state.HasEmojiFont = app.GetSystemFonts().HasEmoji;
    // With --section, menus and popovers open once the section has been scrolled into place, so that they are
    // placed where the screenshot shows them.
    const bool isShowDelayed = !app.GetArguments().Section.empty();
    if (!isShowDelayed)
        state.Show = app.GetArguments().Show;
    state.Artwork = Gallery::CreateArtwork(app.GetHost().GetDevice());

    const std::string& requested = app.GetArguments().Page;
    if (!requested.empty())
    {
        if (const std::optional<Gallery::Page> page = Gallery::FindPage(requested))
        {
            state.CurrentPage = *page;
            state.ShowsPage = true;
        }
        else
            std::fprintf(stderr, "Unknown page '%s'\n", requested.c_str());
    }
    // Screenshots are reproducible: they show a fixed day as today, and can show single sections.
    if (app.GetHost().IsScreenshotMode())
        state.Today = Carbon::DateTime{.Year = 2026, .Month = 10, .Day = 5};
    Gallery::SetCapturedSections(app.GetArguments().Section);
    Example::Host& host = app.GetHost();
    return app.Run(
        [&state, &host, isShowDelayed]
        {
            if (isShowDelayed && host.GetFrameIndex() == Gallery::SectionScreenshotShowFrame)
                state.Show = host.GetArguments().Show;
            Gallery::ResetCapturedArea();
            Gallery::BuildGallery(state);
            const Carbon::Rect area = Gallery::GetCapturedArea();
            if (area.Width > 0.0f)
            {
                const float margin = Gallery::SectionScreenshotMargin;
                host.SetScreenshotArea(area.X - margin, area.Y - margin, area.Width + margin * 2.0f,
                                       area.Height + margin * 2.0f, area.X, area.Y);
            }
        });
}
