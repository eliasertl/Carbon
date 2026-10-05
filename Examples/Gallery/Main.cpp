// Carbon Gallery: every component, in both themes. This is Carbon's visual benchmark; it should look like a
// macOS app without the window chrome.
//
//   Gallery [--theme light|dark] [--scale <factor>] [--size <width>x<height>] [--page <name>]
//           [--screenshot <file.png>] [--show menu|popover|alert|sheet] [--pointer <x>x<y>] [--click <x>x<y>]

#include <cstdio>
#include <vector>

#include "ExampleApp.h"
#include "Pages.h"

namespace Gallery
{
    using namespace Carbon;

    namespace
    {
        struct PageInfo
        {
            Page Id;
            const char* Title;
            /// The name used by --page.
            const char* Key;
            const char* Icon;
        };

        const PageInfo Pages[] = {
            {Page::Typography, "Typography", "typography", Icons::TextAa},
            {Page::Icons, "Icons", "icons", Icons::Star},
            {Page::Buttons, "Buttons", "buttons", Icons::CursorClick},
            {Page::Toggles, "Toggles", "toggles", Icons::ToggleRight},
            {Page::Sliders, "Sliders", "sliders", Icons::SlidersHorizontal},
            {Page::TextFields, "Text Fields", "textfields", Icons::Textbox},
            {Page::Images, "Images", "images", Icons::Image},
            {Page::Layout, "Layout", "layout", Icons::Layout},
            {Page::Selection, "Selection Controls", "selection", Icons::SquaresFour},
            {Page::Dates, "Date and Time", "dates", Icons::CalendarBlank},
            {Page::Menus, "Menus and Popovers", "menus", Icons::List},
            {Page::Toolbars, "Toolbars", "toolbars", Icons::Toolbox},
            {Page::Dialogs, "Alerts and Sheets", "dialogs", Icons::AppWindow},
            {Page::Notifications, "Notifications", "notifications", Icons::Bell},
            {Page::Progress, "Progress", "progress", Icons::CircleNotch},
            {Page::Lists, "Lists and Tables", "lists", Icons::Table},
            {Page::Hierarchies, "Outlines and Columns", "hierarchies", Icons::TreeStructure},
            {Page::Navigation, "Tabs and Split Views", "navigation", Icons::Columns},
            {Page::Charts, "Charts", "charts", Icons::ChartBar},
        };

        // The first page of the components that live in CarbonExtensions.
        constexpr Page FirstExtensionPage = Page::Selection;

        // Screenshots of sections (--section): points between the top of the page and the first captured section,
        // and around the captured area.
        constexpr float SectionScreenshotHeadroom = 40.0f;
        constexpr float SectionScreenshotMargin = 6.0f;
        // The frame in which --show takes effect with --section; scripted pointer input starts at frame 6.
        constexpr int SectionScreenshotShowFrame = 4;

        const PageInfo& GetPageInfo(Page page)
        {
            return Pages[static_cast<size_t>(page)];
        }

        // A small procedural texture, to show that Image displays the host's own texture views.
        wgpu::TextureView CreateArtwork(const wgpu::Device& device)
        {
            const uint32_t size = 128;
            std::vector<uint8_t> pixels(size * size * 4);
            for (uint32_t y = 0; y < size; y++)
            {
                for (uint32_t x = 0; x < size; x++)
                {
                    const float u = float(x) / float(size - 1);
                    const float v = float(y) / float(size - 1);
                    uint8_t* pixel = &pixels[(y * size + x) * 4];
                    pixel[0] = uint8_t(255.0f * (0.35f + 0.65f * u));
                    pixel[1] = uint8_t(255.0f * (0.25f + 0.45f * (1.0f - v)));
                    pixel[2] = uint8_t(255.0f * (0.55f + 0.45f * v));
                    pixel[3] = 255;
                }
            }

            wgpu::TextureDescriptor descriptor;
            descriptor.size = {size, size, 1};
            descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
            descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
            const wgpu::Texture texture = device.CreateTexture(&descriptor);
            wgpu::TexelCopyTextureInfo destination;
            destination.texture = texture;
            wgpu::TexelCopyBufferLayout layout;
            layout.bytesPerRow = size * 4;
            layout.rowsPerImage = size;
            const wgpu::Extent3D extent = {size, size, 1};
            device.GetQueue().WriteTexture(&destination, pixels.data(), pixels.size(), &layout, &extent);
            return texture.CreateView();
        }

        void BuildPage(GalleryState& state)
        {
            switch (state.CurrentPage)
            {
                case Page::Typography:
                    TypographyPage();
                    break;
                case Page::Icons:
                    IconsPage();
                    break;
                case Page::Buttons:
                    ButtonsPage(state);
                    break;
                case Page::Toggles:
                    TogglesPage(state);
                    break;
                case Page::Sliders:
                    SlidersPage(state);
                    break;
                case Page::TextFields:
                    TextFieldsPage(state);
                    break;
                case Page::Images:
                    ImagesPage(state);
                    break;
                case Page::Layout:
                    LayoutPage(state);
                    break;
                case Page::Selection:
                    SelectionPage(state);
                    break;
                case Page::Dates:
                    DatesPage(state);
                    break;
                case Page::Menus:
                    MenusPage(state);
                    break;
                case Page::Toolbars:
                    ToolbarsPage(state);
                    break;
                case Page::Dialogs:
                    DialogsPage(state);
                    break;
                case Page::Notifications:
                    NotificationsPage(state);
                    break;
                case Page::Hierarchies:
                    HierarchiesPage(state);
                    break;
                case Page::Progress:
                    ProgressPage(state);
                    break;
                case Page::Lists:
                    ListsPage(state);
                    break;
                case Page::Navigation:
                    NavigationPage(state);
                    break;
                case Page::Charts:
                    ChartsPage(state);
                    break;
                case Page::Count:
                    break;
            }
        }

        void BuildGallery(GalleryState& state)
        {
            // The whole display: the sidebar, and next to it a header above the scrolling page.
            BeginHStack(
                {.Spacing = 0.0f, .Alignment = VerticalAlignment::Top, .Width = Size::Fill(), .Height = Size::Fill()});

            BeginSidebar("pages", {.Width = 210.0f});
            SidebarHeader("Carbon");
            for (const PageInfo& page : Pages)
            {
                if (page.Id == FirstExtensionPage)
                    SidebarHeader("Extensions");
                if (SidebarItem(page.Title, page.Id == state.CurrentPage, {.Icon = page.Icon}))
                    state.CurrentPage = page.Id;
            }
            EndSidebar();

            BeginVStack({.Spacing = 0.0f, .Width = Size::Fill(), .Height = Size::Fill()});
            const PageInfo& page = GetPageInfo(state.CurrentPage);

            BeginHStack({.Spacing = 16.0f, .Padding = EdgeInsets(24.0f, 14.0f), .Width = Size::Fill()});
            Text(page.Title, {.Style = TextStyle::Title2, .Emphasized = true});
            Spacer();
            if (Toggle("Reduce Motion", &state.ReduceMotion, {.ControlSize = ControlSize::Small}))
                SetReduceMotion(state.ReduceMotion);
            if (Toggle("Dark", &state.IsDark, {.ControlSize = ControlSize::Small}))
                SetTheme(state.IsDark ? Theme::Dark() : Theme::Light());
            EndHStack();
            Separator();

            // Every page has its own scroll view, so each one remembers how far it was scrolled.
            BeginScrollView(page.Key, {.Spacing = 24.0f, .Padding = 24.0f});
            BuildPage(state);
            EndScrollView();
            // A screenshot of single sections (--section) scrolls them to the top, leaving room for a menu above.
            const Rect captured = GetCapturedArea();
            if (captured.Width > 0.0f)
            {
                const float top = GetLastItemRect().Y + SectionScreenshotHeadroom;
                SetScrollOffset(page.Key, Vec2(0.0f, GetScrollOffset(page.Key).Y + captured.Y - top));
            }

            EndVStack();
            EndHStack();

            // Notifications float above everything, wherever they were posted from.
            ShowGalleryNotifications(state);
        }
    } // namespace
} // namespace Gallery

int main(int argc, char** argv)
{
    Example::App app(argc, argv, "Carbon Gallery", 1040, 740);
    if (!app.IsReady())
        return 1;

    Gallery::GalleryState state;
    state.IsDark = app.GetArguments().IsDark;
    // With --section, menus and popovers open once the section has been scrolled into place, so that they are
    // placed where the screenshot shows them.
    const bool isShowDelayed = !app.GetArguments().Section.empty();
    if (!isShowDelayed)
        state.Show = app.GetArguments().Show;
    state.Artwork = Gallery::CreateArtwork(app.GetHost().GetDevice());

    const std::string& requested = app.GetArguments().Page;
    if (!requested.empty())
    {
        bool found = false;
        for (const Gallery::PageInfo& page : Gallery::Pages)
        {
            if (requested == page.Key)
            {
                state.CurrentPage = page.Id;
                found = true;
            }
        }
        if (!found)
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
