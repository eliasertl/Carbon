#include "InputMethod.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#include <string>
#include <vector>

#include <Carbon/Carbon.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// imm.h needs the types of windows.h, so it stays in a block of its own after it.
#include <imm.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace Example
{
#if defined(_WIN32)
    namespace
    {
        // GLFW's window procedure, which the input method's procedure hands every other message to.
        WNDPROC s_GlfwProcedure = nullptr;
        // What the input method was last told, so that it is only told about changes.
        bool s_IsEnabled = true;
        RECT s_CaretRect = {};

        // A composition string of the input context, as UTF-8, with the byte offset of each UTF-16 unit and of
        // the end: IMM32 counts positions in UTF-16 units, Carbon in bytes.
        struct CompositionString
        {
            std::string Text;
            std::vector<size_t> Offsets;
        };

        CompositionString ReadCompositionString(HIMC context, DWORD kind)
        {
            CompositionString result;
            const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
            if (bytes <= 0)
            {
                result.Offsets.push_back(0);
                return result;
            }
            std::wstring units(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
            ImmGetCompositionStringW(context, kind, units.data(), static_cast<DWORD>(bytes));

            for (size_t i = 0; i < units.size(); i++)
            {
                char32_t codepoint = units[i];
                const bool isPair = codepoint >= 0xD800 && codepoint <= 0xDBFF && i + 1 < units.size() &&
                                    units[i + 1] >= 0xDC00 && units[i + 1] <= 0xDFFF;
                result.Offsets.push_back(result.Text.size());
                if (isPair)
                {
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (units[i + 1] - 0xDC00);
                    result.Offsets.push_back(result.Text.size());
                    i++;
                }
                Carbon::AppendUTF8(result.Text, codepoint);
            }
            result.Offsets.push_back(result.Text.size());
            return result;
        }

        size_t ToByteOffset(const CompositionString& string, LONG unit)
        {
            const size_t index = unit < 0 ? 0 : static_cast<size_t>(unit);
            return string.Offsets[index < string.Offsets.size() ? index : string.Offsets.size() - 1];
        }

        // Forwards the pre-edit text with its caret and clauses. A clause is active while the user converts it:
        // its characters carry a target attribute.
        void SendUpdate(HIMC context)
        {
            const CompositionString string = ReadCompositionString(context, GCS_COMPSTR);
            const LONG caret = ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0);

            std::vector<DWORD> boundaries;
            const LONG clauseBytes = ImmGetCompositionStringW(context, GCS_COMPCLAUSE, nullptr, 0);
            if (clauseBytes > 0)
            {
                boundaries.resize(static_cast<size_t>(clauseBytes) / sizeof(DWORD));
                ImmGetCompositionStringW(context, GCS_COMPCLAUSE, boundaries.data(), static_cast<DWORD>(clauseBytes));
            }
            std::vector<BYTE> attributes;
            const LONG attributeBytes = ImmGetCompositionStringW(context, GCS_COMPATTR, nullptr, 0);
            if (attributeBytes > 0)
            {
                attributes.resize(static_cast<size_t>(attributeBytes));
                ImmGetCompositionStringW(context, GCS_COMPATTR, attributes.data(), static_cast<DWORD>(attributeBytes));
            }

            std::vector<Carbon::CompositionClause> clauses;
            for (size_t i = 0; i + 1 < boundaries.size(); i++)
            {
                Carbon::CompositionClause clause;
                clause.Start = ToByteOffset(string, static_cast<LONG>(boundaries[i]));
                clause.End = ToByteOffset(string, static_cast<LONG>(boundaries[i + 1]));
                const size_t first = boundaries[i];
                if (first < attributes.size())
                {
                    const BYTE attribute = attributes[first];
                    clause.IsActive = attribute == ATTR_TARGET_CONVERTED || attribute == ATTR_TARGET_NOTCONVERTED;
                }
                clauses.push_back(clause);
            }
            Carbon::GetIO().AddCompositionUpdateEvent(string.Text, ToByteOffset(string, caret), clauses);
        }

        LRESULT CALLBACK InputMethodProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        {
            switch (message)
            {
                case WM_IME_SETCONTEXT:
                    // Carbon draws the pre-edit text itself; the input method keeps its candidate window.
                    lParam &= ~static_cast<LPARAM>(ISC_SHOWUICOMPOSITIONWINDOW);
                    break;
                case WM_IME_STARTCOMPOSITION:
                    // Not passed on: the default handling would open the input method's own composition window.
                    Carbon::GetIO().AddCompositionStartEvent();
                    return 0;
                case WM_IME_COMPOSITION:
                {
                    // A Korean input method commits a syllable and starts the next one in the same message: the
                    // result comes first. Not passed on, so that the result does not also arrive as WM_CHAR.
                    const HIMC context = ImmGetContext(window);
                    if (context == nullptr)
                        break;
                    if ((lParam & GCS_RESULTSTR) != 0)
                        Carbon::GetIO().AddCompositionCommitEvent(ReadCompositionString(context, GCS_RESULTSTR).Text);
                    if ((lParam & GCS_COMPSTR) != 0)
                        SendUpdate(context);
                    else if ((lParam & GCS_RESULTSTR) == 0)
                        Carbon::GetIO().AddCompositionUpdateEvent("", 0); // the pre-edit text was deleted
                    ImmReleaseContext(window, context);
                    return 0;
                }
                case WM_IME_ENDCOMPOSITION:
                    // After a commit this changes nothing; without one, the user abandoned the composition.
                    Carbon::GetIO().AddCompositionCancelEvent();
                    return 0;
                default:
                    break;
            }
            return CallWindowProcW(s_GlfwProcedure, window, message, wParam, lParam);
        }
    } // namespace

    void InstallInputMethod(GLFWwindow* window)
    {
        if (window == nullptr || s_GlfwProcedure != nullptr)
            return;
        const HWND handle = glfwGetWin32Window(window);
        s_GlfwProcedure = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&InputMethodProcedure)));
    }

    void UpdateInputMethod(GLFWwindow* window)
    {
        if (window == nullptr || s_GlfwProcedure == nullptr)
            return;
        const HWND handle = glfwGetWin32Window(window);
        const Carbon::IO& io = Carbon::GetIO();

        // The input method is on only while a text field is edited, so that keys elsewhere are shortcuts.
        const bool wantsTextInput = io.WantsTextInput();
        if (wantsTextInput != s_IsEnabled)
        {
            ImmAssociateContextEx(handle, nullptr, wantsTextInput ? IACE_DEFAULT : 0);
            s_IsEnabled = wantsTextInput;
            s_CaretRect = {};
        }

        const HIMC context = ImmGetContext(handle);
        if (context == nullptr)
            return;
        if (io.WantsCompositionCancel())
            ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);

        if (wantsTextInput)
        {
            // Carbon's points to client pixels: the framebuffer is the client area.
            int windowWidth = 1;
            int pixelWidth = 1;
            int height = 0;
            glfwGetWindowSize(window, &windowWidth, &height);
            glfwGetFramebufferSize(window, &pixelWidth, &height);
            const float scale = io.GetContentScale() * static_cast<float>(windowWidth) /
                                static_cast<float>(pixelWidth > 0 ? pixelWidth : 1);
            const Carbon::Rect caret = io.GetCaretRect();
            RECT area;
            area.left = static_cast<LONG>(caret.X * scale);
            area.top = static_cast<LONG>(caret.Y * scale);
            area.right = static_cast<LONG>(caret.GetRight() * scale + 1.0f);
            area.bottom = static_cast<LONG>(caret.GetBottom() * scale + 1.0f);
            if (!EqualRect(&area, &s_CaretRect))
            {
                s_CaretRect = area;
                // The candidates go below the caret and never cover its line. Some input methods place them
                // relative to the composition window instead, which Carbon keeps hidden at the same spot.
                CANDIDATEFORM candidate = {};
                candidate.dwIndex = 0;
                candidate.dwStyle = CFS_EXCLUDE;
                candidate.ptCurrentPos = {area.left, area.bottom};
                candidate.rcArea = area;
                ImmSetCandidateWindow(context, &candidate);
                COMPOSITIONFORM composition = {};
                composition.dwStyle = CFS_FORCE_POSITION;
                composition.ptCurrentPos = {area.left, area.top};
                ImmSetCompositionWindow(context, &composition);
            }
        }
        ImmReleaseContext(handle, context);
    }
#else
    void InstallInputMethod(GLFWwindow*) {}

    void UpdateInputMethod(GLFWwindow*) {}
#endif
} // namespace Example
