// Gallery: full frames of the Gallery example. The pages are the example's own code (Examples/Gallery); the
// sidebar and the header around them are rebuilt here as in its Main.cpp.

#include <string>

#include "Pages.h"
#include "Support/FrameBenchmark.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        using Gallery::GalleryState;
        using Gallery::Page;

        struct PageInfo
        {
            Page Id;
            const char* Title;
            void (*Build)(GalleryState& state);
        };

        const PageInfo Pages[] = {
            {Page::Typography, "Typography", [](GalleryState& state) { Gallery::TypographyPage(state); }},
            {Page::Icons, "Icons", [](GalleryState&) { Gallery::IconsPage(); }},
            {Page::Buttons, "Buttons", Gallery::ButtonsPage},
            {Page::Toggles, "Toggles", Gallery::TogglesPage},
            {Page::Sliders, "Sliders", Gallery::SlidersPage},
            {Page::TextFields, "TextFields", Gallery::TextFieldsPage},
            {Page::Layout, "Layout", Gallery::LayoutPage},
            {Page::Selection, "Selection", Gallery::SelectionPage},
            {Page::Dates, "Dates", Gallery::DatesPage},
            {Page::Menus, "Menus", Gallery::MenusPage},
            {Page::Toolbars, "Toolbars", Gallery::ToolbarsPage},
            {Page::Lists, "Lists", Gallery::ListsPage},
            {Page::Hierarchies, "Hierarchies", Gallery::HierarchiesPage},
            {Page::Navigation, "Navigation", Gallery::NavigationPage},
            {Page::Charts, "Charts", Gallery::ChartsPage},
#if defined(CARBON_GALLERY_HAVE_REFLECTION)
            {Page::Reflection, "Reflection", [](GalleryState&) { Gallery::ReflectionPage(); }},
#endif
        };

        void BuildGallery(GalleryState& state, const PageInfo& current)
        {
            BeginHStack(
                {.Spacing = 0.0f, .Alignment = VerticalAlignment::Top, .Width = Size::Fill(), .Height = Size::Fill()});

            BeginSidebar("pages", {.Width = 210.0f});
            SidebarHeader("Carbon");
            for (const PageInfo& page : Pages)
                SidebarItem(page.Title, page.Id == current.Id, {.Icon = Icons::Star});
            EndSidebar();

            BeginVStack({.Spacing = 0.0f, .Width = Size::Fill(), .Height = Size::Fill()});
            BeginHStack({.Spacing = 16.0f, .Padding = EdgeInsets(24.0f, 14.0f), .Width = Size::Fill()});
            Text(current.Title, {.Style = TextStyle::Title2, .Emphasized = true});
            Spacer();
            Toggle("Reduce Motion", &state.ReduceMotion, {.ControlSize = ControlSize::Small});
            Toggle("Dark", &state.IsDark, {.ControlSize = ControlSize::Small});
            EndHStack();
            Separator();

            BeginScrollView(current.Title, {.Spacing = 24.0f, .Padding = 24.0f});
            current.Build(state);
            EndScrollView();

            EndVStack();
            EndHStack();
            Gallery::ShowGalleryNotifications(state);
        }

        void GalleryFrame(benchmark::State& state, const PageInfo* page)
        {
            GalleryState gallery;
            gallery.CurrentPage = page->Id;
            // The day the example's screenshots show, so that the frame is the same on every day.
            gallery.Today = DateTime{.Year = 2026, .Month = 10, .Day = 5};

            // The size of the example's window.
            FrameBenchmark frame({.DisplaySize = Vec2(1040.0f, 740.0f)});
            frame.Measure(state,
                          [&]
                          {
                              Gallery::ResetCapturedArea();
                              BuildGallery(gallery, *page);
                          });
        }

        const int s_Registered = []
        {
            for (const PageInfo& page : Pages)
            {
                benchmark::RegisterBenchmark(std::string("Gallery/") + page.Title, GalleryFrame, &page)
                    ->UseManualTime()
                    ->Unit(benchmark::kMicrosecond);
            }
            return 0;
        }();
    } // namespace
} // namespace Carbon::Benchmarks
