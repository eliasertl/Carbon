#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Input/IO.h"
#include "Carbon/Input/Key.h"
#include "Carbon/Input/MouseButton.h"

namespace Carbon::Internal
{
    /// Seconds a key is held before it starts repeating, and the interval between repeats.
    inline constexpr float KeyRepeatDelay = 0.4f;
    inline constexpr float KeyRepeatInterval = 0.05f;

    /// Maximum time between presses, and maximum travel in points, for presses to count as a multi-click.
    inline constexpr double MultiClickTime = 0.4;
    inline constexpr float MultiClickDistance = 4.0f;

    /// An input method's composition in progress: the pre-edit text the user is composing, which is shown at the
    /// caret of the text being edited but is not part of it.
    struct CompositionState
    {
        bool IsActive = false;
        /// UTF-8; may be empty while a composition is active.
        std::string Text;
        /// Byte offset of the caret in Text.
        size_t Caret = 0;
        /// Ordered, non-overlapping and non-empty; empty when the host sent none.
        std::vector<CompositionClause> Clauses;

        /// Ends the composition. Keeps the storage, so composing does not allocate once it has grown.
        void Clear();
    };

    /// Files from outside the application, dragged over the display or dropped onto it.
    struct FileDragState
    {
        /// Files are being dragged over the display (FileDrag events), until they leave or are dropped.
        bool IsOver = false;
        /// Files were dropped this frame.
        bool IsDropped = false;
        /// The paths, one after the other with a zero byte after each, and views of them.
        std::string Paths;
        std::vector<std::string_view> Files;

        /// Takes the paths of a FileDrag or FileDrop event from the IO object's queue.
        void SetPaths(std::string_view text, size_t count);
    };

    /// The input state of one frame, built in NewFrame from the events the host queued on the IO object.
    struct InputState
    {
        static constexpr size_t MouseButtonCount = static_cast<size_t>(MouseButton::Count);
        static constexpr size_t KeyCount = static_cast<size_t>(Key::Count);

        /// Applies queued events and advances durations by the IO's delta time. Events that would hide an earlier
        /// change within the same frame (press and release of one button, for example) stay queued for the next
        /// frame, so no input is lost at low frame rates.
        void Update(IO& io, double time);

        /// Releases every key and button, as when the host window loses focus.
        void ReleaseAll();

        /// Applies one composition event; the commit's text goes to Characters.
        void ApplyComposition(const IO& io, const InputEvent& event);

        bool HasMousePos = false;
        Vec2 MousePos;
        Vec2 MouseDelta;
        Vec2 MouseWheel;
        std::array<bool, MouseButtonCount> MouseDown{};
        std::array<bool, MouseButtonCount> MousePressed{};
        std::array<bool, MouseButtonCount> MouseReleased{};
        /// Number of consecutive clicks for a press that happened this frame (1 = single, 2 = double); else 0.
        std::array<uint8_t, MouseButtonCount> MouseClickCount{};
        std::array<Vec2, MouseButtonCount> MousePressedPos{};
        std::array<double, MouseButtonCount> MouseLastPressTime{};
        std::array<uint8_t, MouseButtonCount> MouseLastClickCount{};

        std::array<bool, KeyCount> KeyDown{};
        std::array<bool, KeyCount> KeyPressed{};
        std::array<bool, KeyCount> KeyRepeated{};
        std::array<bool, KeyCount> KeyReleased{};
        std::array<float, KeyCount> KeyDownDuration{};
        KeyModifiers Modifiers = KeyModifiers::None;

        /// Typed characters, and the text of compositions committed this frame, in the order they arrived.
        std::vector<char32_t> Characters;
        /// The composition after this frame's events.
        CompositionState Composition;
        /// A composition event arrived this frame.
        bool CompositionChanged = false;
        /// Files dragged in from outside the application.
        FileDragState FileDrag;
        bool Focused = true;
    };
} // namespace Carbon::Internal
