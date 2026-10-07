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

    void AddSystemFallbackFonts()
    {
        // Fallbacks are tried in the order they were added. Japanese comes first, so that the characters Japanese
        // shares with Chinese take their Japanese forms; Chinese fills in simplified characters, Korean the Hangul.
#if defined(_WIN32)
        wchar_t windows[MAX_PATH] = L"C:\\Windows";
        GetWindowsDirectoryW(windows, MAX_PATH);
        const std::filesystem::path fonts = std::filesystem::path(windows) / "Fonts";
        AddFirstFont(fonts, {"YuGothM.ttc", "YuGothR.ttc", "meiryo.ttc", "msgothic.ttc"});
        AddFirstFont(fonts, {"msyh.ttc", "simsun.ttc"});
        AddFirstFont(fonts, {"malgun.ttf", "gulim.ttc"});
#elif defined(__APPLE__)
        AddFirstFont("/System/Library/Fonts", {"Hiragino Sans GB.ttc", "PingFang.ttc"});
        AddFirstFont("/System/Library/Fonts", {"AppleSDGothicNeo.ttc"});
#elif !defined(__EMSCRIPTEN__)
        // Noto Sans CJK covers all three; distributions put it in different places.
        for (const char* folder : {"/usr/share/fonts/opentype/noto", "/usr/share/fonts/noto-cjk",
                                   "/usr/share/fonts/google-noto-cjk", "/usr/share/fonts/OTF"})
        {
            if (AddFirstFont(folder, {"NotoSansCJK-Regular.ttc", "NotoSansCJKjp-Regular.otf"}))
                break;
        }
#endif
    }
} // namespace Example
