#pragma once

#include <cstdint>

struct GLFWwindow;

namespace Example
{
    /// Edges of the window that a resize moves. Combined for corners.
    enum class WindowEdge : uint8_t
    {
        None = 0,
        Left = 1,
        Top = 2,
        Right = 4,
        Bottom = 8
    };

    constexpr WindowEdge operator|(WindowEdge a, WindowEdge b)
    {
        return static_cast<WindowEdge>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    constexpr bool HasEdge(WindowEdge edges, WindowEdge edge)
    {
        return (static_cast<uint8_t>(edges) & static_cast<uint8_t>(edge)) != 0;
    }

    /// The operating-system side of a window without a system title bar: moving, resizing, minimizing,
    /// maximizing and closing. This is host code, which is why it lives in the example and not in Carbon: Carbon
    /// draws the title bar and reports what the user did, the host does it to the window.
    ///
    /// Without a window (screenshot mode) every function does nothing.
    class WindowControls
    {
    public:
        /// `minimumWidth` and `minimumHeight` are in points.
        WindowControls(GLFWwindow* window, float minimumWidth, float minimumHeight);

        /// Continues a move or resize while the left mouse button is held. Call it once per frame.
        void Update();

        /// Starts moving the window with the pointer; it follows until the button is released. A maximized
        /// window is restored first, keeping the pointer at the same place on its title bar.
        void BeginMove();
        /// Starts resizing the window at the given edges.
        void BeginResize(WindowEdge edges);
        /// True while the user moves or resizes the window.
        bool IsMoving() const { return m_Operation == Operation::Move; }
        bool IsResizing() const { return m_Operation == Operation::Resize; }
        WindowEdge GetResizeEdges() const { return m_Edges; }

        void Minimize();
        /// Maximizes the window, or restores it when it is maximized.
        void ToggleMaximize();
        void Close();

        bool IsMaximized() const;
        /// The window has keyboard focus; a title bar is drawn dimmed while it does not.
        bool IsActive() const;
        /// The window can be moved by the host. False on Wayland, where only the compositor moves windows.
        bool CanMove() const;

    private:
        enum class Operation : uint8_t
        {
            None,
            Move,
            Resize
        };

        /// The pointer position in screen coordinates.
        void GetScreenCursor(double& x, double& y) const;

    private:
        GLFWwindow* m_Window = nullptr;
        Operation m_Operation = Operation::None;
        WindowEdge m_Edges = WindowEdge::None;
        // Where the pointer and the window were when the operation started, in screen coordinates.
        double m_StartCursorX = 0.0;
        double m_StartCursorY = 0.0;
        int m_StartX = 0;
        int m_StartY = 0;
        int m_StartWidth = 0;
        int m_StartHeight = 0;
        int m_MinimumWidth = 0;
        int m_MinimumHeight = 0;
    };
} // namespace Example
