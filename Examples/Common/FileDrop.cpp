#include "FileDrop.h"

#include <string>
#include <string_view>
#include <vector>

#include <Carbon/Carbon.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "GlfwInput.h"

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ole2.h>
#include <shellapi.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#endif

namespace Example
{
    namespace
    {
        // The window whose files are forwarded; examples have one.
        GLFWwindow* s_Window = nullptr;

        // Paths as views for IO, which copies them.
        void ForwardFiles(bool isDrop, float x, float y, const std::vector<std::string>& paths)
        {
            std::vector<std::string_view> views(paths.begin(), paths.end());
            if (isDrop)
                Carbon::GetIO().AddFileDropEvent(x, y, views);
            else
                Carbon::GetIO().AddFileDragEvent(x, y, views);
#if !defined(__EMSCRIPTEN__)
            // A host that waits for events renders the frame that shows the change; a page renders every frame.
            glfwPostEmptyEvent();
#endif
        }

#if defined(_WIN32)
        // Reads the paths of the files in a drag (the shell's CF_HDROP format) as UTF-8. Empty when the drag
        // carries no files, such as text dragged from a browser.
        std::vector<std::string> ReadPaths(IDataObject* data)
        {
            std::vector<std::string> paths;
            FORMATETC format = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            STGMEDIUM medium = {};
            if (data == nullptr || FAILED(data->GetData(&format, &medium)))
                return paths;
            HDROP drop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
            if (drop != nullptr)
            {
                const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; i++)
                {
                    std::wstring wide(DragQueryFileW(drop, i, nullptr, 0), L'\0');
                    DragQueryFileW(drop, i, wide.data(), static_cast<UINT>(wide.size() + 1));
                    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                                           nullptr, 0, nullptr, nullptr);
                    std::string path(static_cast<size_t>(length), '\0');
                    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), path.data(), length,
                                        nullptr, nullptr);
                    paths.push_back(std::move(path));
                }
                GlobalUnlock(medium.hGlobal);
            }
            ReleaseStgMedium(&medium);
            return paths;
        }

        // Receives OLE drags over the window: the shell's file manager drags files this way, and it reports the
        // drag as it moves, which GLFW's WM_DROPFILES handling does not.
        class DropTarget final : public IDropTarget
        {
        public:
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override
            {
                if (object == nullptr)
                    return E_POINTER;
                if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IDropTarget))
                {
                    *object = static_cast<IDropTarget*>(this);
                    AddRef();
                    return S_OK;
                }
                *object = nullptr;
                return E_NOINTERFACE;
            }

            ULONG STDMETHODCALLTYPE AddRef() override
            {
                return static_cast<ULONG>(InterlockedIncrement(&m_References));
            }

            ULONG STDMETHODCALLTYPE Release() override
            {
                const LONG references = InterlockedDecrement(&m_References);
                if (references == 0)
                    delete this;
                return static_cast<ULONG>(references);
            }

            HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL point, DWORD* effect) override
            {
                m_Paths = ReadPaths(data);
                return Over(point, effect);
            }

            HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL point, DWORD* effect) override
            {
                return Over(point, effect);
            }

            HRESULT STDMETHODCALLTYPE DragLeave() override
            {
                if (!m_Paths.empty())
                {
                    Carbon::GetIO().AddFileDragLeaveEvent();
                    glfwPostEmptyEvent();
                }
                m_Paths.clear();
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL point, DWORD* effect) override
            {
                m_Paths = ReadPaths(data);
                *effect = m_Paths.empty() ? DROPEFFECT_NONE : DROPEFFECT_COPY;
                if (!m_Paths.empty())
                {
                    float x = 0.0f;
                    float y = 0.0f;
                    ToPoints(point, x, y);
                    ForwardFiles(true, x, y, m_Paths);
                }
                m_Paths.clear();
                return S_OK;
            }

        private:
            HRESULT Over(POINTL point, DWORD* effect)
            {
                *effect = m_Paths.empty() ? DROPEFFECT_NONE : DROPEFFECT_COPY;
                if (m_Paths.empty())
                    return S_OK;
                float x = 0.0f;
                float y = 0.0f;
                ToPoints(point, x, y);
                ForwardFiles(false, x, y, m_Paths);
                return S_OK;
            }

            // OLE reports screen pixels; GLFW's cursor positions are client pixels on Windows.
            static void ToPoints(POINTL point, float& x, float& y)
            {
                POINT client = {point.x, point.y};
                ScreenToClient(glfwGetWin32Window(s_Window), &client);
                CursorToPoints(s_Window, static_cast<double>(client.x), static_cast<double>(client.y), x, y);
            }

        private:
            LONG m_References = 1;
            std::vector<std::string> m_Paths;
        };

        DropTarget* s_DropTarget = nullptr;
        bool s_IsOleInitialized = false;
#endif

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
        void OnGlfwDrop(GLFWwindow* window, int count, const char** paths)
        {
            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetCursorPos(window, &cursorX, &cursorY);
            float x = 0.0f;
            float y = 0.0f;
            CursorToPoints(window, cursorX, cursorY, x, y);
            ForwardFiles(true, x, y, std::vector<std::string>(paths, paths + count));
        }
#endif

#if defined(__EMSCRIPTEN__)
        // The paths of a drop in a browser, passed from the page one at a time through a buffer of this file.
        std::string s_PathBuffer;
        std::vector<std::string> s_DroppedPaths;
#endif
    } // namespace
} // namespace Example

#if defined(__EMSCRIPTEN__)
// Called by the page's drag handlers below. Positions are CSS pixels relative to the canvas, which are the
// window coordinates of the GLFW port.
extern "C"
{
    EMSCRIPTEN_KEEPALIVE char* CarbonExampleFileDropBuffer(int size)
    {
        Example::s_PathBuffer.assign(static_cast<size_t>(size > 0 ? size : 1), '\0');
        return Example::s_PathBuffer.data();
    }

    EMSCRIPTEN_KEEPALIVE void CarbonExampleFileDropAddPath()
    {
        Example::s_DroppedPaths.emplace_back(Example::s_PathBuffer.c_str());
    }

    EMSCRIPTEN_KEEPALIVE void CarbonExampleFileDragOver(double cssX, double cssY, int isDrop)
    {
        if (Example::s_Window == nullptr)
            return;
        float x = 0.0f;
        float y = 0.0f;
        Example::CursorToPoints(Example::s_Window, cssX, cssY, x, y);
        // During the drag a page learns that files come, but not which: their names arrive with the drop.
        Example::ForwardFiles(isDrop != 0, x, y, Example::s_DroppedPaths);
        Example::s_DroppedPaths.clear();
    }

    EMSCRIPTEN_KEEPALIVE void CarbonExampleFileDragLeave()
    {
        Carbon::GetIO().AddFileDragLeaveEvent();
    }
}

// The canvas accepts files: dragover must be cancelled for the browser to allow a drop. The drop's files are
// read (asynchronously) into /dropped of the in-memory file system before Carbon hears of the drop; until then
// the drag goes on, so drop targets stay highlighted while large files load.
// The body is JavaScript, which clang-format would take for C++.
// clang-format off
EM_JS(void, CarbonExampleInstallFileDropHandlers, (), {
    const canvas = Module['canvas'] || document.getElementById('canvas');
    if (!canvas)
        return;
    const hasFiles = (event) => event.dataTransfer && Array.from(event.dataTransfer.types).includes('Files');
    const position = (event) => {
        const bounds = canvas.getBoundingClientRect();
        return [event.clientX - bounds.left, event.clientY - bounds.top];
    };
    canvas.addEventListener('dragover', (event) => {
        if (!hasFiles(event))
            return;
        event.preventDefault();
        event.dataTransfer.dropEffect = 'copy';
        const [x, y] = position(event);
        _CarbonExampleFileDragOver(x, y, 0);
    });
    canvas.addEventListener('dragleave', (event) => {
        if (hasFiles(event))
            _CarbonExampleFileDragLeave();
    });
    canvas.addEventListener('drop', async (event) => {
        if (!hasFiles(event))
            return;
        event.preventDefault();
        const [x, y] = position(event);
        const files = Array.from(event.dataTransfer.files);
        try {
            try {
                FS.mkdir('/dropped');
            } catch (error) {
                // It exists from an earlier drop.
            }
            for (const file of files) {
                const path = '/dropped/' + file.name;
                FS.writeFile(path, new Uint8Array(await file.arrayBuffer()));
                const size = lengthBytesUTF8(path) + 1;
                stringToUTF8(path, _CarbonExampleFileDropBuffer(size), size);
                _CarbonExampleFileDropAddPath();
            }
            _CarbonExampleFileDragOver(x, y, 1);
        } catch (error) {
            console.error('Dropped files could not be read:', error);
            _CarbonExampleFileDragLeave();
        }
    });
});
// clang-format on
EM_JS_DEPS(CarbonExampleFileDrop, "$FS,$stringToUTF8,$lengthBytesUTF8");
#endif

namespace Example
{
    void InstallFileDrop(GLFWwindow* window)
    {
        if (window == nullptr)
            return;
        s_Window = window;
#if defined(_WIN32)
        // OLE needs a single-threaded apartment on this thread. Without one (a host that chose otherwise),
        // GLFW's drop callback is the fallback.
        const HRESULT initialized = OleInitialize(nullptr);
        s_IsOleInitialized = SUCCEEDED(initialized);
        if (s_IsOleInitialized)
        {
            s_DropTarget = new DropTarget();
            if (FAILED(RegisterDragDrop(glfwGetWin32Window(window), s_DropTarget)))
            {
                s_DropTarget->Release();
                s_DropTarget = nullptr;
            }
        }
        if (s_DropTarget == nullptr)
        {
            glfwSetDropCallback(window,
                                [](GLFWwindow* source, int count, const char** paths)
                                {
                                    double cursorX = 0.0;
                                    double cursorY = 0.0;
                                    glfwGetCursorPos(source, &cursorX, &cursorY);
                                    float x = 0.0f;
                                    float y = 0.0f;
                                    CursorToPoints(source, cursorX, cursorY, x, y);
                                    ForwardFiles(true, x, y, std::vector<std::string>(paths, paths + count));
                                });
        }
#elif defined(__EMSCRIPTEN__)
        CarbonExampleInstallFileDropHandlers();
#else
        glfwSetDropCallback(window, OnGlfwDrop);
#endif
    }

    void RemoveFileDrop(GLFWwindow* window)
    {
        if (window == nullptr || window != s_Window)
            return;
#if defined(_WIN32)
        if (s_DropTarget != nullptr)
        {
            RevokeDragDrop(glfwGetWin32Window(window));
            s_DropTarget->Release();
            s_DropTarget = nullptr;
        }
        if (s_IsOleInitialized)
            OleUninitialize();
        s_IsOleInitialized = false;
#endif
        s_Window = nullptr;
    }
} // namespace Example
