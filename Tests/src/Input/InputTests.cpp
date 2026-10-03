#include "Support/ContextTest.h"

#include <string>

namespace Carbon
{
    using InputTests = ContextTest;

    TEST_F(InputTests, DisplayMetricsAreStored)
    {
        IO& io = GetIO();
        io.SetDisplaySize(1280.0f, 720.0f);
        io.SetContentScale(2.0f);
        io.SetDeltaTime(0.01f);
        EXPECT_EQ(io.GetDisplaySize(), Vec2(1280.0f, 720.0f));
        EXPECT_FLOAT_EQ(io.GetContentScale(), 2.0f);
        EXPECT_FLOAT_EQ(io.GetDeltaTime(), 0.01f);
    }

    TEST_F(InputTests, MousePositionAndDelta)
    {
        NewFrame();
        EXPECT_FALSE(IsMousePosValid());
        EndFrame();

        GetIO().AddMousePosEvent(10.0f, 20.0f);
        NewFrame();
        EXPECT_TRUE(IsMousePosValid());
        EXPECT_EQ(GetMousePos(), Vec2(10.0f, 20.0f));
        // The first known position has nothing to be relative to.
        EXPECT_EQ(GetMouseDelta(), Vec2(0.0f, 0.0f));
        EndFrame();

        GetIO().AddMousePosEvent(15.0f, 18.0f);
        NewFrame();
        EXPECT_EQ(GetMouseDelta(), Vec2(5.0f, -2.0f));
        EndFrame();

        GetIO().AddMouseLeaveEvent();
        NewFrame();
        EXPECT_FALSE(IsMousePosValid());
        EndFrame();
    }

    TEST_F(InputTests, ClickProducesPressedThenReleased)
    {
        IO& io = GetIO();
        io.AddMousePosEvent(50.0f, 50.0f);
        io.AddMouseButtonEvent(MouseButton::Left, true);
        NewFrame();
        EXPECT_TRUE(IsMousePressed());
        EXPECT_TRUE(IsMouseDown());
        EXPECT_FALSE(IsMouseReleased());
        EXPECT_EQ(GetMouseClickCount(), 1);
        EXPECT_EQ(GetMousePressedPos(), Vec2(50.0f, 50.0f));
        EXPECT_FALSE(IsMouseDown(MouseButton::Right));
        EndFrame();

        NewFrame();
        EXPECT_FALSE(IsMousePressed());
        EXPECT_TRUE(IsMouseDown());
        EndFrame();

        io.AddMouseButtonEvent(MouseButton::Left, false);
        NewFrame();
        EXPECT_TRUE(IsMouseReleased());
        EXPECT_FALSE(IsMouseDown());
        EndFrame();
    }

    TEST_F(InputTests, PressAndReleaseInOneFrameAreSpreadOverTwo)
    {
        IO& io = GetIO();
        io.AddMousePosEvent(5.0f, 5.0f);
        io.AddMouseButtonEvent(MouseButton::Left, true);
        io.AddMouseButtonEvent(MouseButton::Left, false);

        NewFrame();
        EXPECT_TRUE(IsMousePressed());
        EXPECT_TRUE(IsMouseDown());
        EXPECT_FALSE(IsMouseReleased());
        EndFrame();

        NewFrame();
        EXPECT_TRUE(IsMouseReleased());
        EXPECT_FALSE(IsMouseDown());
        EndFrame();
    }

    TEST_F(InputTests, MoveAfterPressWaitsSoThePressKeepsItsPosition)
    {
        IO& io = GetIO();
        io.AddMousePosEvent(10.0f, 10.0f);
        io.AddMouseButtonEvent(MouseButton::Left, true);
        io.AddMousePosEvent(60.0f, 10.0f);
        io.AddMouseButtonEvent(MouseButton::Left, false);

        NewFrame();
        EXPECT_TRUE(IsMousePressed());
        EXPECT_EQ(GetMousePos(), Vec2(10.0f, 10.0f));
        EndFrame();

        // The drag: the move and the release arrive one frame later, at the new position.
        NewFrame();
        EXPECT_EQ(GetMousePos(), Vec2(60.0f, 10.0f));
        EXPECT_EQ(GetMouseDelta(), Vec2(50.0f, 0.0f));
        EXPECT_EQ(GetMousePos() - GetMousePressedPos(), Vec2(50.0f, 0.0f));
        EXPECT_TRUE(IsMouseReleased());
        EndFrame();
    }

    TEST_F(InputTests, DoubleClickNeedsSpeedAndProximity)
    {
        IO& io = GetIO();
        io.AddMousePosEvent(100.0f, 100.0f);
        io.AddMouseButtonEvent(MouseButton::Left, true);
        NewFrame();
        EXPECT_EQ(GetMouseClickCount(), 1);
        EndFrame();
        io.AddMouseButtonEvent(MouseButton::Left, false);
        RunFrame();

        io.AddMouseButtonEvent(MouseButton::Left, true);
        NewFrame();
        EXPECT_EQ(GetMouseClickCount(), 2);
        EndFrame();
        io.AddMouseButtonEvent(MouseButton::Left, false);
        RunFrame();

        // Too slow: a full second later it is a single click again.
        RunFrame(1.0f);
        io.AddMouseButtonEvent(MouseButton::Left, true);
        NewFrame();
        EXPECT_EQ(GetMouseClickCount(), 1);
        EndFrame();
        io.AddMouseButtonEvent(MouseButton::Left, false);
        RunFrame();

        // Too far away: also a single click.
        io.AddMousePosEvent(140.0f, 100.0f);
        io.AddMouseButtonEvent(MouseButton::Left, true);
        NewFrame();
        EXPECT_EQ(GetMouseClickCount(), 1);
        EndFrame();
    }

    TEST_F(InputTests, WheelAccumulatesWithinAFrameAndResets)
    {
        IO& io = GetIO();
        io.AddMouseWheelEvent(0.0f, -1.0f);
        io.AddMouseWheelEvent(0.5f, -2.0f);
        NewFrame();
        EXPECT_EQ(GetMouseWheel(), Vec2(0.5f, -3.0f));
        EndFrame();

        NewFrame();
        EXPECT_EQ(GetMouseWheel(), Vec2(0.0f, 0.0f));
        EndFrame();
    }

    TEST_F(InputTests, KeyPressReleaseAndModifiers)
    {
        IO& io = GetIO();
        io.AddKeyEvent(Key::LeftShift, true);
        io.AddKeyEvent(Key::Tab, true);
        NewFrame();
        EXPECT_TRUE(IsKeyPressed(Key::Tab));
        EXPECT_TRUE(IsKeyDown(Key::Tab));
        EXPECT_TRUE(HasModifiers(GetKeyModifiers(), KeyModifiers::Shift));
        EXPECT_FALSE(HasModifiers(GetKeyModifiers(), KeyModifiers::Ctrl));
        EndFrame();

        NewFrame();
        EXPECT_FALSE(IsKeyPressed(Key::Tab));
        EXPECT_TRUE(IsKeyDown(Key::Tab));
        EndFrame();

        io.AddKeyEvent(Key::Tab, false);
        io.AddKeyEvent(Key::LeftShift, false);
        NewFrame();
        EXPECT_TRUE(IsKeyReleased(Key::Tab));
        EXPECT_FALSE(IsKeyDown(Key::Tab));
        EXPECT_EQ(GetKeyModifiers(), KeyModifiers::None);
        EndFrame();
    }

    TEST_F(InputTests, HeldKeysRepeatAfterADelay)
    {
        IO& io = GetIO();
        io.AddKeyEvent(Key::RightArrow, true);
        io.AddKeyEvent(Key::RightArrow, true); // host-side repeat is ignored
        NewFrame();
        EXPECT_TRUE(IsKeyPressed(Key::RightArrow));
        EndFrame();

        // Before the delay: held but not repeating.
        int repeats = 0;
        for (int i = 0; i < 3; i++)
        {
            io.SetDeltaTime(0.1f);
            NewFrame();
            repeats += IsKeyPressed(Key::RightArrow) ? 1 : 0;
            EndFrame();
        }
        EXPECT_EQ(repeats, 0);

        // One second of holding at 10 ms frames: the first repeat at 0.4 s, then one every 50 ms.
        repeats = 0;
        int nonRepeatPresses = 0;
        for (int i = 0; i < 100; i++)
        {
            io.SetDeltaTime(0.01f);
            NewFrame();
            repeats += IsKeyPressed(Key::RightArrow, true) ? 1 : 0;
            nonRepeatPresses += IsKeyPressed(Key::RightArrow, false) ? 1 : 0;
            EndFrame();
        }
        EXPECT_GE(repeats, 17);
        EXPECT_LE(repeats, 19);
        EXPECT_EQ(nonRepeatPresses, 0);
    }

    TEST_F(InputTests, ShortcutUsesTheConfiguredModifier)
    {
        IO& io = GetIO();
        io.AddKeyEvent(Key::LeftCtrl, true);
        io.AddKeyEvent(Key::C, true);
        NewFrame();
        EXPECT_TRUE(IsShortcutPressed(Key::C));
        EXPECT_FALSE(IsShortcutPressed(Key::V));
        // Redo (shortcut + Shift) must not fire for a plain shortcut press.
        EXPECT_FALSE(IsShortcutPressed(Key::C, KeyModifiers::Shift));
        EndFrame();

        io.AddKeyEvent(Key::C, false);
        io.AddKeyEvent(Key::LeftCtrl, false);
        RunFrame();

        io.SetShortcutModifier(KeyModifiers::Super);
        io.AddKeyEvent(Key::LeftSuper, true);
        io.AddKeyEvent(Key::C, true);
        NewFrame();
        EXPECT_TRUE(IsShortcutPressed(Key::C));
        EndFrame();
    }

    TEST_F(InputTests, TextInputDecodesUTF8AndDropsControlCharacters)
    {
        GetIO().AddInputCharactersUTF8("a\xC3\xA4\n\xE2\x82\xAC");
        NewFrame();
        const std::span<const char32_t> characters = GetInputCharacters();
        ASSERT_EQ(characters.size(), 3u);
        EXPECT_EQ(characters[0], U'a');
        EXPECT_EQ(characters[1], char32_t(0xE4));
        EXPECT_EQ(characters[2], char32_t(0x20AC));
        EndFrame();

        NewFrame();
        EXPECT_TRUE(GetInputCharacters().empty());
        EndFrame();
    }

    TEST_F(InputTests, TypingKeepsItsOrderRelativeToEditingKeys)
    {
        // "a", Backspace, "b" queued in one frame must not collapse into "ab" plus a Backspace.
        IO& io = GetIO();
        io.AddKeyEvent(Key::A, true);
        io.AddInputCharacter(U'a');
        io.AddKeyEvent(Key::A, false);
        io.AddKeyEvent(Key::Backspace, true);
        io.AddKeyEvent(Key::Backspace, false);
        io.AddKeyEvent(Key::B, true);
        io.AddInputCharacter(U'b');
        io.AddKeyEvent(Key::B, false);

        std::string timeline;
        for (int i = 0; i < 6; i++)
        {
            NewFrame();
            for (char32_t character : GetInputCharacters())
                timeline += static_cast<char>(character);
            if (IsKeyPressed(Key::Backspace))
                timeline += '<';
            EndFrame();
        }
        EXPECT_EQ(timeline, "a<b");
    }

    TEST_F(InputTests, LosingFocusReleasesEverything)
    {
        IO& io = GetIO();
        io.AddKeyEvent(Key::LeftCtrl, true);
        io.AddKeyEvent(Key::A, true);
        io.AddMouseButtonEvent(MouseButton::Left, true);
        NewFrame();
        EXPECT_TRUE(IsKeyDown(Key::A));
        EXPECT_TRUE(IsMouseDown());
        EXPECT_TRUE(IsHostFocused());
        EndFrame();

        io.AddFocusEvent(false);
        NewFrame();
        EXPECT_FALSE(IsHostFocused());
        EXPECT_FALSE(IsKeyDown(Key::A));
        EXPECT_FALSE(IsKeyDown(Key::LeftCtrl));
        EXPECT_TRUE(IsKeyReleased(Key::A));
        EXPECT_FALSE(IsMouseDown());
        EXPECT_TRUE(IsMouseReleased());
        EXPECT_EQ(GetKeyModifiers(), KeyModifiers::None);
        EndFrame();
    }
} // namespace Carbon
