#include "Carbon/Input/Input.h"

#include "Carbon/Core/ContextInternal.h"

namespace Carbon
{
    namespace
    {
        const Internal::InputState& GetInput()
        {
            return Internal::GetContext().Input;
        }

        size_t ToIndex(MouseButton button)
        {
            return static_cast<size_t>(button);
        }

        bool IsValid(Key key)
        {
            return key > Key::None && key < Key::Count;
        }
    } // namespace

    bool IsMousePosValid()
    {
        return GetInput().HasMousePos;
    }

    Vec2 GetMousePos()
    {
        return GetInput().MousePos;
    }

    Vec2 GetMouseDelta()
    {
        return GetInput().MouseDelta;
    }

    Vec2 GetMouseWheel()
    {
        return GetInput().MouseWheel;
    }

    bool IsMouseDown(MouseButton button)
    {
        return button < MouseButton::Count && GetInput().MouseDown[ToIndex(button)];
    }

    bool IsMousePressed(MouseButton button)
    {
        return button < MouseButton::Count && GetInput().MousePressed[ToIndex(button)];
    }

    bool IsMouseReleased(MouseButton button)
    {
        return button < MouseButton::Count && GetInput().MouseReleased[ToIndex(button)];
    }

    int GetMouseClickCount(MouseButton button)
    {
        return button < MouseButton::Count ? GetInput().MouseClickCount[ToIndex(button)] : 0;
    }

    Vec2 GetMousePressedPos(MouseButton button)
    {
        return button < MouseButton::Count ? GetInput().MousePressedPos[ToIndex(button)] : Vec2();
    }

    bool IsKeyDown(Key key)
    {
        return IsValid(key) && GetInput().KeyDown[static_cast<size_t>(key)];
    }

    bool IsKeyPressed(Key key, bool repeat)
    {
        if (!IsValid(key))
            return false;
        const Internal::InputState& input = GetInput();
        const size_t index = static_cast<size_t>(key);
        return input.KeyPressed[index] || (repeat && input.KeyRepeated[index]);
    }

    bool IsKeyReleased(Key key)
    {
        return IsValid(key) && GetInput().KeyReleased[static_cast<size_t>(key)];
    }

    KeyModifiers GetKeyModifiers()
    {
        return GetInput().Modifiers;
    }

    bool IsShortcutPressed(Key key, KeyModifiers extra)
    {
        const Context& context = Internal::GetContext();
        const KeyModifiers expected = context.HostIO.GetShortcutModifier() | extra;
        return context.Input.Modifiers == expected && IsKeyPressed(key, true);
    }

    std::span<const char32_t> GetInputCharacters()
    {
        return GetInput().Characters;
    }

    bool IsHostFocused()
    {
        return GetInput().Focused;
    }
} // namespace Carbon
