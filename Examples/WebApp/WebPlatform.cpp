#include "WebPlatform.h"

#include <cstdlib>

#include <emscripten/emscripten.h>

// EM_ASM's JavaScript refers to its arguments as $0, $1, ..., which pedantic C++ warns about.
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"

// clang-format reads the JavaScript in EM_ASM as C++ and would break it (it splits "!==" into "!= ="), so this file
// is left as written.
// clang-format off

namespace WebApp
{
    std::string GetRoute()
    {
        char* route = static_cast<char*>(EM_ASM_PTR({
            const hash = window.location.hash;
            return stringToNewUTF8(decodeURIComponent(hash.startsWith('#') ? hash.substring(1) : hash));
        }));
        std::string result = route != nullptr ? route : "";
        std::free(route);
        return result;
    }

    void PushRoute(std::string_view route)
    {
        const std::string text(route);
        EM_ASM(
            {
                const hash = '#' + encodeURI(UTF8ToString($0));
                if (window.location.hash !== hash)
                    window.history.pushState(null, "", hash);
            },
            text.c_str());
    }

    bool TakeRouteChange()
    {
        return EM_ASM_INT({
                   // The listener is installed on the first call; it only sets a flag that the next frame reads.
                   if (!Module.carbonRouteListener)
                   {
                       Module.carbonRouteListener = true;
                       Module.carbonRouteChanged = false;
                       window.addEventListener('popstate', function() { Module.carbonRouteChanged = true; });
                   }
                   const changed = Module.carbonRouteChanged;
                   Module.carbonRouteChanged = false;
                   return changed ? 1 : 0;
               }) != 0;
    }

    void SetPageTitle(std::string_view title)
    {
        const std::string text(title);
        EM_ASM({ document.title = UTF8ToString($0); }, text.c_str());
    }

    void OpenUrl(std::string_view url)
    {
        const std::string text(url);
        EM_ASM({ window.open(UTF8ToString($0), '_blank', 'noopener'); }, text.c_str());
    }

    bool PrefersDarkAppearance()
    {
        return EM_ASM_INT({
                   return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? 1 : 0;
               }) != 0;
    }
} // namespace WebApp
// clang-format on
