#include "WebHost.h"

#if defined(__EMSCRIPTEN__)

#include <algorithm>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

#include <emscripten/emscripten.h>

// EM_ASM's JavaScript refers to its arguments as $0, $1, ..., which pedantic C++ warns about.
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"

// clang-format reads the JavaScript in EM_JS and EM_ASM as C++ and would break it (it splits "!==" into "!= ="), so
// this file is left as written.
// clang-format off

namespace
{
    // Text from the page: the page writes it here (CarbonWebBuffer) before it calls one of the functions below.
    std::string s_Buffer;

    // The keys a page's input element passes on, in the order the page numbers them.
    constexpr Carbon::Key WebKeys[] = {Carbon::Key::Enter,     Carbon::Key::Tab,        Carbon::Key::Escape,
                                       Carbon::Key::Backspace, Carbon::Key::Delete,     Carbon::Key::LeftArrow,
                                       Carbon::Key::RightArrow, Carbon::Key::UpArrow,   Carbon::Key::DownArrow,
                                       Carbon::Key::Home,      Carbon::Key::End,        Carbon::Key::LeftShift};
} // namespace

extern "C"
{
    EMSCRIPTEN_KEEPALIVE char* CarbonWebBuffer(int size)
    {
        s_Buffer.assign(static_cast<size_t>(size > 0 ? size : 0), '\0');
        return s_Buffer.data();
    }

    // phase: 0 began, 1 moved, 2 ended, 3 cancelled. Positions are CSS pixels from the canvas's corner, which are
    // the examples' points.
    EMSCRIPTEN_KEEPALIVE void CarbonWebTouch(int phase, double id, double x, double y, int isPen)
    {
        if (Carbon::GetCurrentContext() == nullptr || phase < 0 || phase > 3)
            return;
        Carbon::GetIO().AddTouchEvent(static_cast<Carbon::TouchPhase>(phase), static_cast<uint64_t>(id),
                                      static_cast<float>(x), static_cast<float>(y),
                                      isPen != 0 ? Carbon::PointerType::Pen : Carbon::PointerType::Touch);
    }

    // The keyboard replaced [start, end) of the text being edited with the buffer's text (UTF-8 byte offsets).
    EMSCRIPTEN_KEEPALIVE void CarbonWebReplace(int start, int end)
    {
        if (Carbon::GetCurrentContext() == nullptr)
            return;
        Carbon::GetIO().AddTextReplaceEvent(static_cast<size_t>(start), static_cast<size_t>(end),
                                            std::string_view(s_Buffer.c_str()));
    }

    // kind: 0 start, 1 update (the buffer's text, the caret at its end), 2 commit (the buffer's text), 3 cancel.
    EMSCRIPTEN_KEEPALIVE void CarbonWebComposition(int kind)
    {
        if (Carbon::GetCurrentContext() == nullptr)
            return;
        Carbon::IO& io = Carbon::GetIO();
        const std::string_view text(s_Buffer.c_str());
        switch (kind)
        {
            case 0:
                io.AddCompositionStartEvent();
                break;
            case 1:
                io.AddCompositionUpdateEvent(text, text.size());
                break;
            case 2:
                io.AddCompositionCommitEvent(text);
                break;
            default:
                io.AddCompositionCancelEvent();
                break;
        }
    }

    EMSCRIPTEN_KEEPALIVE void CarbonWebKey(int key, int down)
    {
        if (Carbon::GetCurrentContext() == nullptr || key < 0 || key >= static_cast<int>(std::size(WebKeys)))
            return;
        Carbon::GetIO().AddKeyEvent(WebKeys[key], down != 0);
    }
}

// The page's side. Listeners on the window's capture phase run before those of the GLFW port (on the document and
// the canvas), so that fingers and pens, and keys typed into the hidden input element, reach Carbon only this way;
// a mouse is left to GLFW.
EM_JS(void, CarbonWebInstall, (), {
    const canvas = Module['canvas'] || document.getElementById('canvas');
    const host = {
        input: null, mirror: "", composing: false, lastEdit: 0, areas: [], wantsText: false,
        keyboardType: 0, isSecure: false
    };
    Module['carbonWebHost'] = host;

    const sendText = (text) => {
        const size = lengthBytesUTF8(text) + 1;
        stringToUTF8(text, _CarbonWebBuffer(size), size);
    };
    const bytes = (text, index) => lengthBytesUTF8(text.substring(0, index));
    const toPoint = (event) => {
        const rect = canvas.getBoundingClientRect();
        return [event.clientX - rect.left, event.clientY - rect.top];
    };

    // The hidden input element: what the on-screen keyboard types into. It sits at Carbon's caret, invisible, with
    // 16-pixel text so that iOS does not zoom the page when it gets the focus.
    const input = document.createElement('input');
    input.type = 'text';
    input.setAttribute('autocomplete', 'off');
    input.setAttribute('aria-hidden', 'true');
    input.style.cssText = 'position:fixed;left:0;top:0;width:2px;height:24px;margin:0;padding:0;border:0;' +
                          'opacity:0;font-size:16px;color:transparent;background:transparent;caret-color:transparent;' +
                          'pointer-events:none;z-index:-1;';
    document.body.appendChild(input);
    host.input = input;

    const focusInput = () => {
        if (document.activeElement !== input)
            input.focus({ preventScroll: true });
    };
    host.focus = focusInput;
    // The keyboard goes away: the canvas takes the keys again.
    host.blur = () => {
        if (document.activeElement === input)
            canvas.focus({ preventScroll: true });
    };
    // Focus moving between the canvas and the input element stays inside the application: GLFW must not tell Carbon
    // that its window lost the focus (which hides the caret and releases every key and finger).
    const isInside = (element) => element === canvas || element === input;
    for (const type of ['blur', 'focusout', 'focus', 'focusin'])
    {
        window.addEventListener(type, (event) => {
            if (isInside(event.target) && isInside(event.relatedTarget))
                event.stopPropagation();
        }, true);
    }
    const isInTextArea = (x, y) => {
        for (const area of host.areas)
        {
            if (x >= area[0] && y >= area[1] && x <= area[0] + area[2] && y <= area[1] + area[3])
                return true;
        }
        return false;
    };

    // Fingers and pens. A mouse goes through GLFW, as on the desktop.
    const phases = { pointerdown: 0, pointermove: 1, pointerup: 2, pointercancel: 3 };
    const onPointer = (event) => {
        if (event.pointerType === 'mouse' || event.target !== canvas)
            return;
        event.preventDefault();
        event.stopPropagation();
        // A pen that hovers moves without touching: there is no hover on a touchscreen.
        if (event.type === 'pointermove' && event.buttons === 0)
            return;
        if (event.type === 'pointerdown')
            canvas.setPointerCapture(event.pointerId);
        const point = toPoint(event);
        _CarbonWebTouch(phases[event.type], event.pointerId, point[0], point[1], event.pointerType === 'pen' ? 1 : 0);
        // iOS shows the keyboard only for an element focused while the tap is handled: decide from where Carbon drew
        // its text fields in the last frame.
        if (event.type === 'pointerup' && isInTextArea(point[0], point[1]))
            focusInput();
    };
    for (const type of Object.keys(phases))
        window.addEventListener(type, onPointer, { capture: true, passive: false });
    // The touch events of the same fingers would scroll or zoom the page and reach GLFW as a mouse.
    const onTouch = (event) => {
        if (event.target !== canvas)
            return;
        event.preventDefault();
        event.stopPropagation();
        if (event.type === 'touchend')
        {
            for (const touch of event.changedTouches)
            {
                const point = toPoint(touch);
                if (isInTextArea(point[0], point[1]))
                    focusInput();
            }
        }
    };
    for (const type of ['touchstart', 'touchmove', 'touchend', 'touchcancel'])
        window.addEventListener(type, onTouch, { capture: true, passive: false });

    // Keys typed into the input element. Text arrives through its input events; only the keys that act on the text
    // as a whole go to Carbon as keys. GLFW does not see any of them.
    const keys = { Enter: 0, Tab: 1, Escape: 2, Backspace: 3, Delete: 4, ArrowLeft: 5, ArrowRight: 6, ArrowUp: 7,
                   ArrowDown: 8, Home: 9, End: 10, Shift: 11 };
    const onKey = (event) => {
        if (event.target !== input)
            return;
        event.stopPropagation();
        if (event.type === 'keypress' || host.composing || event.isComposing || event.keyCode === 229)
            return;
        const key = keys[event.key];
        if (key === undefined)
            return;
        const isDown = event.type === 'keydown';
        // Deleting inside the text changes the element's value, which goes to Carbon as an edit. At the start (or
        // the end) nothing changes there, but a token field deletes its last token.
        if (event.key === 'Backspace' && !(input.selectionStart === 0 && input.selectionEnd === 0))
            return;
        if (event.key === 'Delete' && !(input.selectionStart === input.value.length && input.selectionEnd === input.value.length))
            return;
        if (event.key !== 'Shift')
            event.preventDefault();
        _CarbonWebKey(key, isDown ? 1 : 0);
    };
    for (const type of ['keydown', 'keyup', 'keypress'])
        window.addEventListener(type, onKey, true);

    // What the keyboard changed: the difference to the text Carbon has, as one replacement.
    input.addEventListener('input', (event) => {
        if (host.composing || event.isComposing)
            return;
        const before = host.mirror;
        const after = input.value;
        if (before === after)
            return;
        let start = 0;
        while (start < before.length && start < after.length && before[start] === after[start])
            start++;
        let tail = 0;
        while (tail < before.length - start && tail < after.length - start &&
               before[before.length - 1 - tail] === after[after.length - 1 - tail])
            tail++;
        sendText(after.substring(start, after.length - tail));
        _CarbonWebReplace(bytes(before, start), bytes(before, before.length - tail));
        host.mirror = after;
        host.lastEdit = performance.now();
    });
    input.addEventListener('compositionstart', () => {
        host.composing = true;
        sendText("");
        _CarbonWebComposition(0);
    });
    input.addEventListener('compositionupdate', (event) => {
        sendText(event.data || "");
        _CarbonWebComposition(1);
    });
    input.addEventListener('compositionend', (event) => {
        host.composing = false;
        sendText(event.data || "");
        _CarbonWebComposition(2);
        host.mirror = input.value;
        host.lastEdit = performance.now();
    });

    // The page never scrolls: the canvas is the whole interface. A browser that scrolls to show the focused input
    // is scrolled back; Carbon moves the text above the keyboard itself.
    const unscroll = () => { if (window.scrollX !== 0 || window.scrollY !== 0) window.scrollTo(0, 0); };
    window.addEventListener('scroll', unscroll);
    if (window.visualViewport)
        window.visualViewport.addEventListener('scroll', unscroll);

    // Probes for what CSS knows: the safe area and the text size.
    const safe = document.createElement('div');
    safe.style.cssText = 'position:fixed;visibility:hidden;pointer-events:none;' +
                         'padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left);';
    document.body.appendChild(safe);
    host.safe = safe;
    const text = document.createElement('div');
    text.style.cssText = 'position:fixed;visibility:hidden;pointer-events:none;';
    // iOS: the Dynamic Type size of body text, which is 17 pixels at the default size.
    if (CSS.supports('font', '-apple-system-body'))
    {
        text.style.font = '-apple-system-body';
        host.textBase = 17;
    }
    else
    {
        // Elsewhere: the browser's default font size, 16 pixels unless the user changed it.
        text.style.fontSize = 'medium';
        host.textBase = 16;
    }
    document.body.appendChild(text);
    host.text = text;
});
EM_JS_DEPS(CarbonWebHost, "$stringToUTF8,$lengthBytesUTF8,$UTF8ToString");

// Reads the page's metrics into `values`: safe area (left, top, right, bottom), text scale, keyboard height, and
// whether the device's main pointer is a finger (1) or not (0).
EM_JS(void, CarbonWebReadMetrics, (float* values), {
    const host = Module['carbonWebHost'];
    if (!host)
        return;
    const safe = getComputedStyle(host.safe);
    HEAPF32[(values >> 2) + 0] = parseFloat(safe.paddingLeft) || 0;
    HEAPF32[(values >> 2) + 1] = parseFloat(safe.paddingTop) || 0;
    HEAPF32[(values >> 2) + 2] = parseFloat(safe.paddingRight) || 0;
    HEAPF32[(values >> 2) + 3] = parseFloat(safe.paddingBottom) || 0;
    const size = parseFloat(getComputedStyle(host.text).fontSize) || host.textBase;
    HEAPF32[(values >> 2) + 4] = size / host.textBase;
    // The keyboard covers what the visual viewport leaves of the layout viewport at its bottom.
    let keyboard = 0;
    if (window.visualViewport)
    {
        const viewport = window.visualViewport;
        keyboard = document.documentElement.clientHeight - (viewport.offsetTop + viewport.height);
        // Toolbars that come and go change it by less.
        if (keyboard < 80)
            keyboard = 0;
    }
    HEAPF32[(values >> 2) + 5] = keyboard;
    const isTouchDevice = window.matchMedia && window.matchMedia('(pointer: coarse)').matches &&
                          !window.matchMedia('(any-pointer: fine)').matches;
    HEAPF32[(values >> 2) + 6] = isTouchDevice ? 1 : 0;
});

// Tells the page what Carbon edits: where the caret is, the text and selection (UTF-8 offsets), what keyboard it
// wants, and where the text fields are.
EM_JS(void, CarbonWebSync, (int wantsText, float x, float y, float height, const char* textPointer, int selectionStart,
                            int selectionEnd, int keyboardType, int isSecure, const float* areas, int areaCount), {
    const host = Module['carbonWebHost'];
    if (!host)
        return;
    host.areas = [];
    for (let i = 0; i < areaCount; i++)
    {
        const base = (areas >> 2) + i * 4;
        host.areas.push([HEAPF32[base], HEAPF32[base + 1], HEAPF32[base + 2], HEAPF32[base + 3]]);
    }
    host.wantsText = wantsText !== 0;
    if (!host.wantsText)
        return;
    const input = host.input;

    // Under the caret, kept inside what is visible, so the browser has no reason to scroll.
    const viewport = window.visualViewport;
    const bottom = viewport ? viewport.offsetTop + viewport.height : window.innerHeight;
    input.style.left = Math.max(0, x) + 'px';
    input.style.top = Math.max(0, Math.min(y, bottom - height)) + 'px';
    input.style.height = Math.max(height, 16) + 'px';

    // The keyboard Carbon asks for.
    if (keyboardType !== host.keyboardType || (isSecure !== 0) !== host.isSecure)
    {
        host.keyboardType = keyboardType;
        host.isSecure = isSecure !== 0;
        input.type = host.isSecure ? 'password' : 'text';
        const modes = ['text', 'decimal', 'email', 'url', 'search'];
        input.setAttribute('inputmode', modes[keyboardType] || 'text');
        const plain = host.isSecure || keyboardType !== 0;
        input.setAttribute('autocorrect', plain ? 'off' : 'on');
        input.setAttribute('autocapitalize', plain ? 'off' : 'sentences');
        input.setAttribute('spellcheck', plain ? 'false' : 'true');
        input.setAttribute('enterkeyhint', keyboardType === 4 ? 'search' : 'done');
    }

    // Carbon's text and selection, unless the keyboard is still changing them: what was typed a moment ago may not
    // have reached Carbon yet.
    if (host.composing || performance.now() - host.lastEdit < 300)
        return;
    const text = UTF8ToString(textPointer);
    if (input.value !== text)
        input.value = text;
    host.mirror = text;
    const toIndex = (offset) => {
        let index = 0;
        let count = 0;
        while (index < text.length && count < offset)
        {
            count += lengthBytesUTF8(text[index]);
            index++;
        }
        return index;
    };
    const start = toIndex(selectionStart);
    const end = toIndex(selectionEnd);
    if (document.activeElement === input && (input.selectionStart !== start || input.selectionEnd !== end))
        input.setSelectionRange(start, end);
});

namespace Example
{
    void InstallWebHost(Carbon::Callbacks& callbacks)
    {
        // Carbon asks for the keyboard when a text control starts being edited. A finger already focused the input
        // element while its tap was handled (iOS insists on that); a mouse leaves the keyboard alone.
        callbacks.SetKeyboardVisible = [](bool visible)
        {
            if (visible && Carbon::IsTouchMode())
                EM_ASM({ const host = Module['carbonWebHost']; if (host) host.focus(); });
            else
                EM_ASM({ const host = Module['carbonWebHost']; if (host) host.blur(); });
        };
    }

    void StartWebHost()
    {
        CarbonWebInstall();
    }

    void UpdateWebHostBeforeFrame()
    {
        float values[7] = {};
        CarbonWebReadMetrics(values);
        Carbon::IO& io = Carbon::GetIO();
        io.SetSafeAreaInsets(Carbon::EdgeInsets(values[0], values[1], values[2], values[3]));
        io.SetTextScale(values[4] > 0.0f ? values[4] : 1.0f);
        const Carbon::Vec2 display = io.GetDisplaySize();
        const float keyboard = std::min(values[5], display.Y);
        io.SetKeyboardRect(keyboard > 0.0f ? Carbon::Rect(0.0f, display.Y - keyboard, display.X, keyboard)
                                           : Carbon::Rect());
        io.SetDefaultPointerType(values[6] > 0.5f ? Carbon::PointerType::Touch : Carbon::PointerType::Mouse);
    }

    void UpdateWebHostAfterFrame()
    {
        const Carbon::IO& io = Carbon::GetIO();
        const Carbon::TextInputState& state = io.GetTextInputState();
        const Carbon::Rect caret = io.GetCaretRect();
        const std::span<const Carbon::Rect> areas = io.GetTextInputAreas();
        // The input element follows the caret only while a finger edits; a mouse and its keyboard use GLFW.
        const bool wantsText = io.WantsTextInput() && Carbon::GetPointerType() != Carbon::PointerType::Mouse;
        // Rect is four floats, so the areas go to the page as they are.
        static_assert(sizeof(Carbon::Rect) == sizeof(float) * 4);
        // A zero-terminated copy of the text, in storage that keeps its capacity.
        static std::string s_Text;
        s_Text.assign(state.Text);
        CarbonWebSync(wantsText ? 1 : 0, caret.X, caret.Y, caret.Height, s_Text.c_str(),
                      static_cast<int>(state.SelectionStart), static_cast<int>(state.SelectionEnd),
                      static_cast<int>(state.Keyboard), state.IsSecure ? 1 : 0,
                      reinterpret_cast<const float*>(areas.data()), static_cast<int>(areas.size()));
    }
} // namespace Example
// clang-format on

#else

namespace Example
{
    void InstallWebHost(Carbon::Callbacks&) {}

    void StartWebHost() {}

    void UpdateWebHostBeforeFrame() {}

    void UpdateWebHostAfterFrame() {}
} // namespace Example

#endif
