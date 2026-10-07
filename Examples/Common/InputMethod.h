#pragma once

struct GLFWwindow;

namespace Example
{
    /// Connects the window's input method to the current Carbon context, so that Japanese, Chinese, Korean and
    /// other text typed through one is composed inline in Carbon's text fields. GLFW has no input method API, so
    /// this talks to the platform directly: IMM32 on Windows. Elsewhere it does nothing, and text arrives as
    /// characters, as before. Does nothing for a null window (screenshot mode).
    void InstallInputMethod(GLFWwindow* window);

    /// Called after EndFrame: turns the input method on while Carbon edits text and off otherwise, places its
    /// candidate window at Carbon's caret, and cancels a composition Carbon ended itself.
    void UpdateInputMethod(GLFWwindow* window);
} // namespace Example
