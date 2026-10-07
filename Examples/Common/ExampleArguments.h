#pragma once

#include <string>

namespace Example
{
    /// Command-line options shared by all examples.
    struct Arguments
    {
        /// --screenshot <file.png>: render offscreen, save the image and exit. No window is opened.
        std::string ScreenshotPath;
        /// --theme light|dark
        bool IsDark = false;
        /// --scale <factor>: content scale; 0 uses the monitor's scale (1 in screenshot mode).
        float Scale = 0.0f;
        /// --size <width>x<height>: size in points; 0 uses the example's default. Handy for tall screenshots.
        int Width = 0;
        int Height = 0;
        /// Set by the example, not on the command line: open the window without the system's frame and title bar,
        /// because the example draws its own.
        bool IsFrameless = false;
        /// --page <name>: the page an example with several pages starts on.
        std::string Page;
        /// --show <name>: something the example opens at startup, such as a menu or a sheet.
        std::string Show;
        /// --pointer <x>x<y>, --click <x>x<y>, --right-click <x>x<y>: in screenshot mode, puts the pointer at a
        /// position in points and optionally clicks there, so that hover states and menus can be captured. The
        /// position is relative to the window, or to the cropped area's anchor when the example crops (--section).
        float PointerX = -1.0f;
        float PointerY = -1.0f;
        /// 0 for the left button, 1 for the right one, -1 for no click.
        int ClickButton = -1;
        /// --compose <text>: in screenshot mode, after the click, an input method composes this text in the focused
        /// text field: '|' separates its clauses, the first of which is being converted, and \uXXXX or
        /// \UXXXXXXXX stands for a character.
        std::string Composition;
        /// --crop <x>,<y>,<width>,<height>: in screenshot mode, saves only this area of the window, in points.
        float Crop[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        /// --section <key>[,<key>...]: in screenshot mode, an example with sections (the Gallery) scrolls to them
        /// and saves only their area. Keys are section titles in lower case without spaces or punctuation.
        std::string Section;
        /// --extend <left>,<top>,<right>,<bottom>: points added to the saved area on each side, for menus and
        /// popovers that reach out of a section.
        float Extend[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    /// Parses the options above. Unknown options are reported on stderr and ignored.
    Arguments ParseArguments(int argc, char** argv);

    /// Screenshot mode renders this many frames before it saves the last one, so that animations and first-frame
    /// layout have settled; each frame advances time by ScreenshotDeltaTime.
    inline constexpr int ScreenshotWarmupFrames = 16;
    inline constexpr float ScreenshotDeltaTime = 0.25f;
} // namespace Example
