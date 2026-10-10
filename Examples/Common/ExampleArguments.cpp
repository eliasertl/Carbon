#include "ExampleArguments.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace Example
{
    namespace
    {
        // Reads `count` numbers separated by commas, such as "10,20,300,200".
        bool ParseNumbers(const char* text, float* values, int count)
        {
            for (int i = 0; i < count; i++)
            {
                char* end = nullptr;
                values[i] = std::strtof(text, &end);
                if (end == text || (i + 1 < count && *end != ',') || (i + 1 == count && *end != '\0'))
                    return false;
                text = end + 1;
            }
            return true;
        }
    } // namespace

    Arguments ParseArguments(int argc, char** argv)
    {
        Arguments arguments;
        for (int i = 1; i < argc; i++)
        {
            const std::string_view option = argv[i];
            const bool hasValue = i + 1 < argc;
            if (option == "--screenshot" && hasValue)
            {
                arguments.ScreenshotPath = argv[++i];
            }
            else if (option == "--theme" && hasValue)
            {
                const std::string_view theme = argv[++i];
                if (theme != "light" && theme != "dark")
                    std::fprintf(stderr, "Unknown theme '%s'; expected light or dark\n", argv[i]);
                arguments.IsDark = theme == "dark";
            }
            else if (option == "--scale" && hasValue)
            {
                arguments.Scale = static_cast<float>(std::atof(argv[++i]));
                if (arguments.Scale <= 0.0f)
                {
                    std::fprintf(stderr, "Invalid scale '%s'\n", argv[i]);
                    arguments.Scale = 0.0f;
                }
            }
            else if (option == "--size" && hasValue)
            {
                const std::string size = argv[++i];
                const size_t separator = size.find('x');
                if (separator != std::string::npos)
                {
                    arguments.Width = std::atoi(size.substr(0, separator).c_str());
                    arguments.Height = std::atoi(size.substr(separator + 1).c_str());
                }
                if (arguments.Width <= 0 || arguments.Height <= 0)
                {
                    std::fprintf(stderr, "Invalid size '%s'; expected <width>x<height>\n", argv[i]);
                    arguments.Width = 0;
                    arguments.Height = 0;
                }
            }
            else if (option == "--page" && hasValue)
            {
                arguments.Page = argv[++i];
            }
            else if (option == "--show" && hasValue)
            {
                arguments.Show = argv[++i];
            }
            else if ((option == "--crop" || option == "--extend") && hasValue)
            {
                float* values = option == "--crop" ? arguments.Crop : arguments.Extend;
                if (!ParseNumbers(argv[++i], values, 4))
                {
                    std::fprintf(stderr, "Invalid area '%s'; expected four numbers separated by commas\n", argv[i]);
                    std::fill(values, values + 4, 0.0f);
                }
            }
            else if (option == "--compose" && hasValue)
            {
                arguments.Composition = argv[++i];
            }
            else if (option == "--section" && hasValue)
            {
                arguments.Section = argv[++i];
            }
            else if (option == "--drag" && hasValue)
            {
                float position[2] = {-1.0f, -1.0f};
                const std::string text = argv[++i];
                const size_t separator = text.find('x');
                if (separator != std::string::npos)
                {
                    position[0] = static_cast<float>(std::atof(text.substr(0, separator).c_str()));
                    position[1] = static_cast<float>(std::atof(text.substr(separator + 1).c_str()));
                }
                else
                {
                    std::fprintf(stderr, "Invalid position '%s'; expected <x>x<y>\n", argv[i]);
                }
                arguments.DragX = position[0];
                arguments.DragY = position[1];
            }
            else if ((option == "--pointer" || option == "--click" || option == "--right-click") && hasValue)
            {
                arguments.ClickButton = option == "--click" ? 0 : (option == "--right-click" ? 1 : -1);
                const std::string position = argv[++i];
                const size_t separator = position.find('x');
                if (separator != std::string::npos)
                {
                    arguments.PointerX = static_cast<float>(std::atof(position.substr(0, separator).c_str()));
                    arguments.PointerY = static_cast<float>(std::atof(position.substr(separator + 1).c_str()));
                }
                else
                {
                    std::fprintf(stderr, "Invalid position '%s'; expected <x>x<y>\n", argv[i]);
                }
            }
            else
            {
                std::fprintf(stderr, "Unknown option '%s'\n", argv[i]);
                std::fprintf(
                    stderr,
                    "Options: --screenshot <file.png>  --theme light|dark  --scale <factor>  "
                    "--size <width>x<height>  --page <name>  --show <name>  --pointer <x>x<y>  "
                    "--click <x>x<y>  --right-click <x>x<y>  --drag <x>x<y>  --compose <text>  --crop <x>,<y>,<w>,<h>  "
                    "--section <key>[,<key>...]  --extend <left>,<top>,<right>,<bottom>\n");
            }
        }
        return arguments;
    }
} // namespace Example
