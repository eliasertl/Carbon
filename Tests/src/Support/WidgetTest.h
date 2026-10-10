#pragma once

#include "Support/ContextTest.h"

namespace Carbon
{
    /// Fixture for interaction tests: simulates the host's input the way a real application delivers it, one
    /// step per frame, while rebuilding the interface each frame.
    class WidgetTest : public ContextTest
    {
    protected:
        using Builder = std::function<void()>;

        /// Moves the pointer and runs two frames: one for the item under it to claim the pointer, one in which
        /// that item reports being hovered.
        void MoveMouse(Vec2 position, const Builder& build)
        {
            GetIO().AddMousePosEvent(position.X, position.Y);
            Frame(build);
            Frame(build);
        }

        void PressMouse(const Builder& build, MouseButton button = MouseButton::Left)
        {
            GetIO().AddMouseButtonEvent(button, true);
            Frame(build);
        }

        void ReleaseMouse(const Builder& build, MouseButton button = MouseButton::Left)
        {
            GetIO().AddMouseButtonEvent(button, false);
            Frame(build);
        }

        /// Moves to `position`, presses and releases the left button.
        void Click(Vec2 position, const Builder& build)
        {
            MoveMouse(position, build);
            PressMouse(build);
            ReleaseMouse(build);
        }

        /// A finger touches the display: one frame in which the pointer arrives, one in which it presses.
        void TouchDown(Vec2 position, const Builder& build, uint64_t finger = 1)
        {
            GetIO().AddTouchEvent(TouchPhase::Began, finger, position.X, position.Y);
            Frame(build);
            Frame(build);
        }

        /// A finger moves, one frame.
        void TouchMove(Vec2 position, const Builder& build, uint64_t finger = 1)
        {
            GetIO().AddTouchEvent(TouchPhase::Moved, finger, position.X, position.Y);
            Frame(build);
        }

        /// A finger is lifted: one frame in which it releases, one in which the pointer leaves.
        void TouchUp(Vec2 position, const Builder& build, uint64_t finger = 1)
        {
            GetIO().AddTouchEvent(TouchPhase::Ended, finger, position.X, position.Y);
            Frame(build);
            Frame(build);
        }

        /// Touches and lifts a finger at one point.
        void Tap(Vec2 position, const Builder& build)
        {
            TouchDown(position, build);
            TouchUp(position, build);
        }

        /// Drags a finger from where it is down to `to` in `steps` frames, in a straight line.
        void TouchDrag(Vec2 from, Vec2 to, int steps, const Builder& build, uint64_t finger = 1)
        {
            for (int i = 1; i <= steps; i++)
                TouchMove(from + (to - from) * (static_cast<float>(i) / static_cast<float>(steps)), build, finger);
        }

        /// Presses and releases a key, one frame each.
        void TapKey(Key key, const Builder& build)
        {
            GetIO().AddKeyEvent(key, true);
            Frame(build);
            GetIO().AddKeyEvent(key, false);
            Frame(build);
        }

        /// Taps a key while a modifier key is held.
        void TapKey(Key modifier, Key key, const Builder& build)
        {
            GetIO().AddKeyEvent(modifier, true);
            Frame(build);
            TapKey(key, build);
            GetIO().AddKeyEvent(modifier, false);
            Frame(build);
        }

        /// Types text as the OS would deliver it: as characters.
        void Type(std::string_view text, const Builder& build)
        {
            GetIO().AddInputCharactersUTF8(text);
            Frame(build);
        }

        /// The primitive a vertex of the last frame's draw data refers to.
        const DrawPrimitive& GetPrimitive(size_t vertex) const
        {
            const DrawData& drawData = GetDrawData();
            return drawData.Primitives[drawData.Vertices[vertex].Primitive];
        }

        /// The color of the first quad of the last frame, unpacked.
        Color GetQuadColor(size_t quad) const
        {
            const uint32_t packed = GetDrawData().Vertices[quad * 4].Color;
            return Color::FromRGBA8(static_cast<uint8_t>(packed & 0xFF), static_cast<uint8_t>((packed >> 8) & 0xFF),
                                    static_cast<uint8_t>((packed >> 16) & 0xFF), static_cast<uint8_t>(packed >> 24));
        }
    };
} // namespace Carbon
