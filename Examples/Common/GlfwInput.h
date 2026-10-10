#pragma once

#include <Carbon/Carbon.h>

struct GLFWwindow;

namespace Example
{
    /// Maps a GLFW key code (GLFW_KEY_*) to Carbon's key enumeration; Key::None for keys Carbon does not use.
    Carbon::Key ToCarbonKey(int glfwKey);

    /// Maps a GLFW mouse button (GLFW_MOUSE_BUTTON_*) to Carbon's; returns false for buttons Carbon ignores.
    bool ToCarbonMouseButton(int glfwButton, Carbon::MouseButton& button);

    /// Converts a GLFW cursor position (screen coordinates) to points, with the content scale last set on the
    /// current context's IO.
    void CursorToPoints(GLFWwindow* window, double cursorX, double cursorY, float& x, float& y);

    /// Installs GLFW callbacks on a window that forward mouse, keyboard, text, focus and file drop events to the
    /// current Carbon context. Does nothing for a null window (screenshot mode).
    void InstallInputCallbacks(GLFWwindow* window);

    /// Fills in the clipboard and cursor callbacks of a context description using a GLFW window. For a null
    /// window (screenshot mode) the callbacks stay empty.
    void InstallPlatformCallbacks(GLFWwindow* window, Carbon::Callbacks& callbacks);
} // namespace Example
