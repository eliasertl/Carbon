#pragma once

#include <cstdint>

namespace Carbon
{
    /// Keyboard keys, named by their position on a US layout. The host maps its own key codes to these.
    enum class Key : uint16_t
    {
        None,

        Tab,
        LeftArrow,
        RightArrow,
        UpArrow,
        DownArrow,
        PageUp,
        PageDown,
        Home,
        End,
        Insert,
        Delete,
        Backspace,
        Space,
        Enter,
        Escape,
        Menu,

        LeftCtrl,
        LeftShift,
        LeftAlt,
        LeftSuper,
        RightCtrl,
        RightShift,
        RightAlt,
        RightSuper,

        D0,
        D1,
        D2,
        D3,
        D4,
        D5,
        D6,
        D7,
        D8,
        D9,

        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,

        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,

        Apostrophe,
        Comma,
        Minus,
        Period,
        Slash,
        Semicolon,
        Equal,
        LeftBracket,
        Backslash,
        RightBracket,
        GraveAccent,

        KeypadEnter,

        Count
    };

    /// Modifier keys held during an event; combine with `|`.
    enum class KeyModifiers : uint8_t
    {
        None = 0,
        Ctrl = 1 << 0,
        Shift = 1 << 1,
        Alt = 1 << 2,
        Super = 1 << 3
    };

    constexpr KeyModifiers operator|(KeyModifiers a, KeyModifiers b)
    {
        return static_cast<KeyModifiers>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    constexpr KeyModifiers operator&(KeyModifiers a, KeyModifiers b)
    {
        return static_cast<KeyModifiers>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    /// True when every modifier in `flags` is set in `modifiers`.
    constexpr bool HasModifiers(KeyModifiers modifiers, KeyModifiers flags)
    {
        return (modifiers & flags) == flags;
    }
} // namespace Carbon
