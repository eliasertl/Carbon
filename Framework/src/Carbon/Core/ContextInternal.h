#pragma once

#include <cstdint>
#include <vector>

#include "Carbon/Core/Callbacks.h"
#include "Carbon/Core/ContentScale.h"
#include "Carbon/Core/ID.h"
#include "Carbon/Draw/DrawList.h"
#include "Carbon/Input/IO.h"
#include "Carbon/Input/InputStateInternal.h"

namespace Carbon
{
    /// All state of one Carbon instance. Internal: modules reach it through Internal::GetContext().
    struct Context
    {
        Callbacks HostCallbacks;
        IO HostIO;
        Internal::InputState Input;

        /// The ID scopes; the first entry is the root scope and is never popped.
        std::vector<ID> IDStack;
        DrawList Draw;

        ContentScale Scale;
        Vec2 DisplaySize;
        float DeltaTime = 0.0f;
        double Time = 0.0;
        uint64_t FrameCount = 0;
        bool IsInFrame = false;
    };

    namespace Internal
    {
        /// The current context; null when none exists.
        extern Context* g_CurrentContext;

        /// Returns the current context. Calling a Carbon function without one is a usage error.
        Context& GetContext();

        /// Returns the current context and verifies that a frame is in progress (between NewFrame and EndFrame).
        Context& GetFrameContext();
    } // namespace Internal
} // namespace Carbon
