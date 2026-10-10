#pragma once

struct GLFWwindow;

namespace Example
{
    /// Forwards files dragged from the system onto the window to the current Carbon context
    /// (IO::AddFileDragEvent, AddFileDragLeaveEvent and AddFileDropEvent), so that Carbon's drop targets for files
    /// highlight while files are over them and receive them when they are dropped:
    ///
    /// - Windows: an OLE drop target, which reports the drag as it moves, with the paths, and then the drop.
    /// - In a browser: the canvas's drag events. A page gets no paths, only the files' contents: dropped files are
    ///   written to Emscripten's in-memory file system under /dropped, and those are the paths Carbon reports.
    /// - Elsewhere: GLFW's drop callback, which reports only the drop, at the cursor.
    ///
    /// Does nothing for a null window (screenshot mode).
    void InstallFileDrop(GLFWwindow* window);

    /// Undoes InstallFileDrop before the window is destroyed.
    void RemoveFileDrop(GLFWwindow* window);
} // namespace Example
