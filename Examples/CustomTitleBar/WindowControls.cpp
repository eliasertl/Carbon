#include "WindowControls.h"

#include <algorithm>
#include <cmath>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#endif

namespace Example
{
    WindowControls::WindowControls(GLFWwindow* window, float minimumWidth, float minimumHeight) : m_Window(window)
    {
        if (m_Window == nullptr)
            return;

        // Window sizes are in screen coordinates, which are pixels on Windows and X11.
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        glfwGetWindowContentScale(m_Window, &scaleX, &scaleY);
        m_MinimumWidth = static_cast<int>(std::lround(minimumWidth * scaleX));
        m_MinimumHeight = static_cast<int>(std::lround(minimumHeight * scaleY));
        glfwSetWindowSizeLimits(m_Window, m_MinimumWidth, m_MinimumHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);

#if defined(_WIN32)
        // Windows 11 rounds the corners of framed windows. A frameless one asks for the same; older versions
        // ignore the request.
        const HWND handle = glfwGetWin32Window(m_Window);
        const DWORD DwmWindowCornerPreference = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
        const int RoundCorners = 2;                 // DWMWCP_ROUND
        DwmSetWindowAttribute(handle, DwmWindowCornerPreference, &RoundCorners, sizeof(RoundCorners));
#endif
    }

    void WindowControls::GetScreenCursor(double& x, double& y) const
    {
        double cursorX = 0.0;
        double cursorY = 0.0;
        int windowX = 0;
        int windowY = 0;
        glfwGetCursorPos(m_Window, &cursorX, &cursorY);
        glfwGetWindowPos(m_Window, &windowX, &windowY);
        x = windowX + cursorX;
        y = windowY + cursorY;
    }

    void WindowControls::Update()
    {
        if (m_Window == nullptr || m_Operation == Operation::None)
            return;
        if (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS)
        {
            m_Operation = Operation::None;
            m_Edges = WindowEdge::None;
            return;
        }

        double cursorX = 0.0;
        double cursorY = 0.0;
        GetScreenCursor(cursorX, cursorY);
        const int deltaX = static_cast<int>(std::lround(cursorX - m_StartCursorX));
        const int deltaY = static_cast<int>(std::lround(cursorY - m_StartCursorY));

        if (m_Operation == Operation::Move)
        {
            glfwSetWindowPos(m_Window, m_StartX + deltaX, m_StartY + deltaY);
            return;
        }

        // Resizing: the dragged edges follow the pointer, the opposite ones stay where they are.
        int x = m_StartX;
        int y = m_StartY;
        int width = m_StartWidth;
        int height = m_StartHeight;
        if (HasEdge(m_Edges, WindowEdge::Right))
            width = std::max(m_StartWidth + deltaX, m_MinimumWidth);
        if (HasEdge(m_Edges, WindowEdge::Bottom))
            height = std::max(m_StartHeight + deltaY, m_MinimumHeight);
        if (HasEdge(m_Edges, WindowEdge::Left))
        {
            width = std::max(m_StartWidth - deltaX, m_MinimumWidth);
            x = m_StartX + m_StartWidth - width;
        }
        if (HasEdge(m_Edges, WindowEdge::Top))
        {
            height = std::max(m_StartHeight - deltaY, m_MinimumHeight);
            y = m_StartY + m_StartHeight - height;
        }

        int currentX = 0;
        int currentY = 0;
        glfwGetWindowPos(m_Window, &currentX, &currentY);
        if (currentX != x || currentY != y)
            glfwSetWindowPos(m_Window, x, y);
        glfwSetWindowSize(m_Window, width, height);
    }

    void WindowControls::BeginMove()
    {
        if (m_Window == nullptr || !CanMove())
            return;

        if (IsMaximized())
        {
            // Dragging a maximized window restores it under the pointer, where the pointer was on its title bar.
            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetCursorPos(m_Window, &cursorX, &cursorY);
            int width = 1;
            int height = 1;
            glfwGetWindowSize(m_Window, &width, &height);
            const double fraction = std::clamp(cursorX / std::max(width, 1), 0.0, 1.0);

            double screenX = 0.0;
            double screenY = 0.0;
            GetScreenCursor(screenX, screenY);
            glfwRestoreWindow(m_Window);
            glfwGetWindowSize(m_Window, &width, &height);
            glfwSetWindowPos(m_Window, static_cast<int>(std::lround(screenX - fraction * width)),
                             static_cast<int>(std::lround(screenY - cursorY)));
        }

        GetScreenCursor(m_StartCursorX, m_StartCursorY);
        glfwGetWindowPos(m_Window, &m_StartX, &m_StartY);
        m_Operation = Operation::Move;
    }

    void WindowControls::BeginResize(WindowEdge edges)
    {
        if (m_Window == nullptr || edges == WindowEdge::None || IsMaximized())
            return;
        GetScreenCursor(m_StartCursorX, m_StartCursorY);
        glfwGetWindowPos(m_Window, &m_StartX, &m_StartY);
        glfwGetWindowSize(m_Window, &m_StartWidth, &m_StartHeight);
        m_Edges = edges;
        m_Operation = Operation::Resize;
    }

    void WindowControls::Minimize()
    {
        if (m_Window != nullptr)
            glfwIconifyWindow(m_Window);
    }

    void WindowControls::ToggleMaximize()
    {
        if (m_Window == nullptr)
            return;
        if (IsMaximized())
            glfwRestoreWindow(m_Window);
        else
            glfwMaximizeWindow(m_Window);
    }

    void WindowControls::Close()
    {
        if (m_Window != nullptr)
            glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
    }

    bool WindowControls::IsMaximized() const
    {
        return m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) == GLFW_TRUE;
    }

    bool WindowControls::IsActive() const
    {
        // Screenshots show an active window.
        return m_Window == nullptr || glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) == GLFW_TRUE;
    }

    bool WindowControls::CanMove() const
    {
        return m_Window != nullptr && glfwGetPlatform() != GLFW_PLATFORM_WAYLAND;
    }
} // namespace Example
