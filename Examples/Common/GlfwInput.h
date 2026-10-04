#pragma once

#include <Carbon/Carbon.h>

struct GLFWwindow;

namespace Example
{
    class Host;

    /// Maps a GLFW key code (GLFW_KEY_*) to Carbon's key enumeration; Key::None for keys Carbon does not use.
    Carbon::Key ToCarbonKey(int glfwKey);

    /// Maps a GLFW mouse button (GLFW_MOUSE_BUTTON_*) to Carbon's; returns false for buttons Carbon ignores.
    bool ToCarbonMouseButton(int glfwButton, Carbon::MouseButton& button);

    /// Installs GLFW callbacks on the host's window that forward mouse, keyboard, text and focus events to the
    /// current Carbon context. Does nothing in screenshot mode (there is no window).
    void InstallInputCallbacks(const Host& host);

    /// Fills in the clipboard and cursor callbacks of a context description using the host's GLFW window. In
    /// screenshot mode the callbacks stay empty.
    void InstallPlatformCallbacks(const Host& host, Carbon::Callbacks& callbacks);
} // namespace Example
