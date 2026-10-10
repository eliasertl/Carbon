#include "Gallery.h"

#include <vector>

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
            {Page::DragAndDrop, "Drag and Drop", "draganddrop", Icons::HandGrabbing},
            {Page::Navigation, "Tabs and Split Views", "navigation", Icons::Columns},
            {Page::Charts, "Charts", "charts", Icons::ChartBar},
#if defined(CARBON_GALLERY_HAVE_REFLECTION)
            {Page::Reflection, "Reflection", "reflection", Icons::BracketsCurly},
#endif
        };

        // The first page of the components that live in CarbonExtensions.
        constexpr Page FirstExtensionPage = Page::Selection;

        const PageInfo& GetPageInfo(Page page)
        {
            return Pages[static_cast<size_t>(page)];
        }

        void BuildPage(GalleryState& state)
        {
            switch (state.CurrentPage)
            {
                case Page::Typography:
                    TypographyPage(state);
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
                case Page::DragAndDrop:
                    DragDropPage(state);
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
#if defined(CARBON_GALLERY_HAVE_REFLECTION)
                case Page::Reflection:
                    ReflectionPage();
                    break;
#endif
                case Page::Count:
                    break;
            }
        }
    } // namespace

    bool BuildGallery(GalleryState& state, const GalleryOptions& options)
    {
        bool isBackClicked = false;

        // The whole display: the sidebar, and next to it a header above the scrolling page. On a phone (compact
        // width) the sidebar is the list of pages, and a page slides in over it with a back button.
        const PageInfo& page = GetPageInfo(state.CurrentPage);
        if (state.ShowsPage)
        {
            ShowNavigationDetail("gallery", true, false);
            state.ShowsPage = false;
        }
        BeginNavigationSplitView("gallery", {.Title = "Gallery", .DetailTitle = page.Title});

        BeginSidebar("pages", {.Width = 210.0f});
        SidebarHeader("Carbon");
        for (const PageInfo& entry : Pages)
        {
            if (entry.Id == FirstExtensionPage)
                SidebarHeader("Extensions");
            if (SidebarItem(entry.Title, entry.Id == state.CurrentPage, {.Icon = entry.Icon}))
                state.CurrentPage = entry.Id;
        }
        EndSidebar();

        NavigationSplitViewDetail();
        const bool isCompact = IsCompactWidth();
        BeginHStack({.Spacing = 16.0f, .Padding = EdgeInsets(isCompact ? 16.0f : 24.0f, 14.0f), .Width = Size::Fill()});
        if (options.HasBackButton && !isCompact)
        {
            isBackClicked = Button("##back", {.Role = ButtonRole::Plain, .Icon = Icons::CaretLeft});
            Tooltip("Back to the start");
        }
        // In compact width the navigation bar shows the title.
        if (!isCompact)
            Text(page.Title, {.Style = TextStyle::Title2, .Emphasized = true});
        Spacer();
        if (Toggle("Reduce Motion", &state.ReduceMotion, {.ControlSize = ControlSize::Small}))
            SetReduceMotion(state.ReduceMotion);
        if (Toggle("Dark", &state.IsDark, {.ControlSize = ControlSize::Small}))
            SetTheme(state.IsDark ? Theme::Dark() : Theme::Light());
        EndHStack();
        Separator();

        // Every page has its own scroll view, so each one remembers how far it was scrolled.
        BeginScrollView(page.Key, {.Spacing = 24.0f, .Padding = isCompact ? 16.0f : 24.0f});
        BuildPage(state);
        EndScrollView();
        // A screenshot of single sections (--section) scrolls them to the top, leaving room for a menu above.
        const Rect captured = GetCapturedArea();
        if (captured.Width > 0.0f)
        {
            const float top = GetLastItemRect().Y + SectionScreenshotHeadroom;
            SetScrollOffset(page.Key, Vec2(0.0f, GetScrollOffset(page.Key).Y + captured.Y - top));
        }

        EndNavigationSplitView();

        // Notifications float above everything, wherever they were posted from.
        ShowGalleryNotifications(state);
        return isBackClicked;
    }

    std::optional<Page> FindPage(std::string_view key)
    {
        for (const PageInfo& page : Pages)
        {
            if (key == page.Key)
                return page.Id;
        }
        return std::nullopt;
    }

    std::string_view GetPageKey(Page page)
    {
        return GetPageInfo(page).Key;
    }

    Carbon::TextureID CreateArtwork(Example::GraphicsDevice& device)
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
        return device.CreateTexture(size, size, pixels);
    }
} // namespace Gallery
