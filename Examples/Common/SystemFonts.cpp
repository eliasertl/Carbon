#include "SystemFonts.h"

#include <filesystem>
#include <initializer_list>

#include <Carbon/Carbon.h>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Example
{
#if !defined(__EMSCRIPTEN__)
    // A page has no system fonts to look for.
    namespace
    {
        // Adds the first of the candidate files that exists. Returns false when none does.
        bool AddFirstFont(const std::filesystem::path& folder, std::initializer_list<const char*> candidates)
        {
            std::error_code error;
            for (const char* candidate : candidates)
            {
                const std::filesystem::path path = folder / candidate;
                if (std::filesystem::is_regular_file(path, error) && Carbon::AddFontFromFile(path) != nullptr)
                    return true;
            }
            return false;
        }
    } // namespace
#endif

    SystemFallbackFonts AddSystemFallbackFonts()
    {
        SystemFallbackFonts found;
        // Fallbacks are tried in the order they were added. Japanese comes first, so that the characters Japanese
        // shares with Chinese take their Japanese forms; Chinese fills in simplified characters, Korean the Hangul.
#if defined(_WIN32)
        wchar_t windows[MAX_PATH] = L"C:\\Windows";
        GetWindowsDirectoryW(windows, MAX_PATH);
        const std::filesystem::path fonts = std::filesystem::path(windows) / "Fonts";
        found.HasCjk = AddFirstFont(fonts, {"YuGothM.ttc", "YuGothR.ttc", "meiryo.ttc", "msgothic.ttc"});
        found.HasCjk = AddFirstFont(fonts, {"msyh.ttc", "simsun.ttc"}) || found.HasCjk;
        found.HasCjk = AddFirstFont(fonts, {"malgun.ttf", "gulim.ttc"}) || found.HasCjk;
        // Segoe UI Emoji: COLR version 1, with version 0 layers for older renderers.
        found.HasEmoji = AddFirstFont(fonts, {"seguiemj.ttf"});
#elif defined(__APPLE__)
        found.HasCjk = AddFirstFont("/System/Library/Fonts", {"Hiragino Sans GB.ttc", "PingFang.ttc"});
        found.HasCjk = AddFirstFont("/System/Library/Fonts", {"AppleSDGothicNeo.ttc"}) || found.HasCjk;
        // Apple Color Emoji: sbix images.
        found.HasEmoji = AddFirstFont("/System/Library/Fonts", {"Apple Color Emoji.ttc"});
#elif !defined(__EMSCRIPTEN__)
        // Noto Sans CJK covers all three; distributions put it in different places.
        for (const char* folder : {"/usr/share/fonts/opentype/noto", "/usr/share/fonts/noto-cjk",
                                   "/usr/share/fonts/google-noto-cjk", "/usr/share/fonts/OTF"})
        {
            found.HasCjk = AddFirstFont(folder, {"NotoSansCJK-Regular.ttc", "NotoSansCJKjp-Regular.otf"});
            if (found.HasCjk)
                break;
        }
        // Noto Color Emoji: CBDT images in most distributions' package.
        for (const char* folder : {"/usr/share/fonts/truetype/noto", "/usr/share/fonts/noto",
                                   "/usr/share/fonts/google-noto-emoji", "/usr/share/fonts/google-noto-color-emoji"})
        {
            found.HasEmoji = AddFirstFont(folder, {"NotoColorEmoji.ttf"});
            if (found.HasEmoji)
                break;
        }
#endif
        return found;
    }
} // namespace Example
