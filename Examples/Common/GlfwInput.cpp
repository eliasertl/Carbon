#include "GlfwInput.h"

#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Example
{
    Carbon::Key ToCarbonKey(int glfwKey)
    {
        using Carbon::Key;

        if (glfwKey >= GLFW_KEY_A && glfwKey <= GLFW_KEY_Z)
            return static_cast<Key>(static_cast<int>(Key::A) + (glfwKey - GLFW_KEY_A));
        if (glfwKey >= GLFW_KEY_0 && glfwKey <= GLFW_KEY_9)
            return static_cast<Key>(static_cast<int>(Key::D0) + (glfwKey - GLFW_KEY_0));
        if (glfwKey >= GLFW_KEY_F1 && glfwKey <= GLFW_KEY_F12)
            return static_cast<Key>(static_cast<int>(Key::F1) + (glfwKey - GLFW_KEY_F1));

        switch (glfwKey)
        {
            case GLFW_KEY_TAB:
                return Key::Tab;
            case GLFW_KEY_LEFT:
                return Key::LeftArrow;
            case GLFW_KEY_RIGHT:
                return Key::RightArrow;
            case GLFW_KEY_UP:
                return Key::UpArrow;
            case GLFW_KEY_DOWN:
                return Key::DownArrow;
            case GLFW_KEY_PAGE_UP:
                return Key::PageUp;
            case GLFW_KEY_PAGE_DOWN:
                return Key::PageDown;
            case GLFW_KEY_HOME:
                return Key::Home;
            case GLFW_KEY_END:
                return Key::End;
            case GLFW_KEY_INSERT:
                return Key::Insert;
            case GLFW_KEY_DELETE:
                return Key::Delete;
            case GLFW_KEY_BACKSPACE:
                return Key::Backspace;
            case GLFW_KEY_SPACE:
                return Key::Space;
            case GLFW_KEY_ENTER:
                return Key::Enter;
            case GLFW_KEY_KP_ENTER:
                return Key::KeypadEnter;
            case GLFW_KEY_ESCAPE:
                return Key::Escape;
            case GLFW_KEY_MENU:
                return Key::Menu;
            case GLFW_KEY_LEFT_CONTROL:
                return Key::LeftCtrl;
            case GLFW_KEY_LEFT_SHIFT:
                return Key::LeftShift;
            case GLFW_KEY_LEFT_ALT:
                return Key::LeftAlt;
            case GLFW_KEY_LEFT_SUPER:
                return Key::LeftSuper;
            case GLFW_KEY_RIGHT_CONTROL:
                return Key::RightCtrl;
            case GLFW_KEY_RIGHT_SHIFT:
                return Key::RightShift;
            case GLFW_KEY_RIGHT_ALT:
                return Key::RightAlt;
            case GLFW_KEY_RIGHT_SUPER:
                return Key::RightSuper;
            case GLFW_KEY_APOSTROPHE:
                return Key::Apostrophe;
            case GLFW_KEY_COMMA:
                return Key::Comma;
            case GLFW_KEY_MINUS:
                return Key::Minus;
            case GLFW_KEY_PERIOD:
                return Key::Period;
            case GLFW_KEY_SLASH:
                return Key::Slash;
            case GLFW_KEY_SEMICOLON:
                return Key::Semicolon;
            case GLFW_KEY_EQUAL:
                return Key::Equal;
            case GLFW_KEY_LEFT_BRACKET:
                return Key::LeftBracket;
            case GLFW_KEY_BACKSLASH:
                return Key::Backslash;
            case GLFW_KEY_RIGHT_BRACKET:
                return Key::RightBracket;
            case GLFW_KEY_GRAVE_ACCENT:
                return Key::GraveAccent;
            default:
                return Key::None;
        }
    }

    bool ToCarbonMouseButton(int glfwButton, Carbon::MouseButton& button)
    {
        switch (glfwButton)
        {
            case GLFW_MOUSE_BUTTON_LEFT:
                button = Carbon::MouseButton::Left;
                return true;
            case GLFW_MOUSE_BUTTON_RIGHT:
                button = Carbon::MouseButton::Right;
                return true;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                button = Carbon::MouseButton::Middle;
                return true;
            case GLFW_MOUSE_BUTTON_4:
                button = Carbon::MouseButton::Back;
                return true;
            case GLFW_MOUSE_BUTTON_5:
                button = Carbon::MouseButton::Forward;
                return true;
            default:
                return false;
        }
    }

    void CursorToPoints(GLFWwindow* window, double cursorX, double cursorY, float& x, float& y)
    {
        // GLFW reports the cursor in screen coordinates, which are pixels on Windows and X11 but may be scaled
        // units elsewhere. Going through the framebuffer size handles both; the content scale is the one the host
        // gave Carbon.
        int windowWidth = 1;
        int windowHeight = 1;
        int pixelWidth = 1;
        int pixelHeight = 1;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        glfwGetFramebufferSize(window, &pixelWidth, &pixelHeight);
        const double pixelsPerUnitX = windowWidth > 0 ? static_cast<double>(pixelWidth) / windowWidth : 1.0;
        const double pixelsPerUnitY = windowHeight > 0 ? static_cast<double>(pixelHeight) / windowHeight : 1.0;
        const double contentScale = Carbon::GetIO().GetContentScale();
        x = static_cast<float>(cursorX * pixelsPerUnitX / contentScale);
        y = static_cast<float>(cursorY * pixelsPerUnitY / contentScale);
    }

    void InstallInputCallbacks(GLFWwindow* window)
    {
        if (window == nullptr)
            return;

        glfwSetCursorPosCallback(window,
                                 [](GLFWwindow* source, double cursorX, double cursorY)
                                 {
                                     float x = 0.0f;
                                     float y = 0.0f;
                                     CursorToPoints(source, cursorX, cursorY, x, y);
                                     Carbon::GetIO().AddMousePosEvent(x, y);
                                 });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow*, int entered)
                                   {
                                       if (entered == GLFW_FALSE)
                                           Carbon::GetIO().AddMouseLeaveEvent();
                                   });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow*, int glfwButton, int action, int)
                                   {
                                       Carbon::MouseButton button;
                                       if (ToCarbonMouseButton(glfwButton, button))
                                           Carbon::GetIO().AddMouseButtonEvent(button, action != GLFW_RELEASE);
                                   });
        glfwSetScrollCallback(
            window, [](GLFWwindow*, double scrollX, double scrollY)
            { Carbon::GetIO().AddMouseWheelEvent(static_cast<float>(scrollX), static_cast<float>(scrollY)); });
        glfwSetKeyCallback(window,
                           [](GLFWwindow*, int glfwKey, int, int action, int)
                           {
                               // Carbon repeats held keys itself, so GLFW_REPEAT is not forwarded.
                               if (action != GLFW_REPEAT)
                                   Carbon::GetIO().AddKeyEvent(ToCarbonKey(glfwKey), action == GLFW_PRESS);
                           });
        glfwSetCharCallback(window, [](GLFWwindow*, unsigned int codepoint)
                            { Carbon::GetIO().AddInputCharacter(static_cast<char32_t>(codepoint)); });
        glfwSetWindowFocusCallback(
            window, [](GLFWwindow*, int focused) { Carbon::GetIO().AddFocusEvent(focused == GLFW_TRUE); });
    }

    void InstallPlatformCallbacks(GLFWwindow* window, Carbon::Callbacks& callbacks)
    {
        if (window == nullptr)
            return;

        callbacks.GetClipboardText = [window]() -> std::string
        {
            const char* text = glfwGetClipboardString(window);
            return text != nullptr ? std::string(text) : std::string();
        };
        callbacks.SetClipboardText = [window](std::string_view text)
        { glfwSetClipboardString(window, std::string(text).c_str()); };
        callbacks.SetCursor = [window](Carbon::Cursor cursor)
        {
            // The standard cursors are created once and live until glfwTerminate.
            static constexpr int Count = 8;
            static GLFWcursor* cursors[Count] = {};
            static const int Shapes[Count] = {
                GLFW_ARROW_CURSOR,     GLFW_IBEAM_CURSOR,       GLFW_POINTING_HAND_CURSOR, GLFW_RESIZE_EW_CURSOR,
                GLFW_RESIZE_NS_CURSOR, GLFW_NOT_ALLOWED_CURSOR, GLFW_RESIZE_NWSE_CURSOR,   GLFW_RESIZE_NESW_CURSOR};
            const int index = static_cast<int>(cursor);
            if (index < 0 || index >= Count)
                return;
            if (cursors[index] == nullptr)
                cursors[index] = glfwCreateStandardCursor(Shapes[index]);
            glfwSetCursor(window, cursors[index]);
        };
    }
} // namespace Example
