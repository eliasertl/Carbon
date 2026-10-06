#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "Carbon/Core/Callbacks.h"
#include "Carbon/Core/ContentScale.h"
#include "Carbon/Core/ID.h"
#include "Carbon/Core/StateStorageInternal.h"
#include "Carbon/Draw/DrawList.h"
#include "Carbon/Input/IO.h"
#include "Carbon/Input/InputStateInternal.h"
#include "Carbon/Interaction/InteractionInternal.h"
#include "Carbon/Layout/LayoutInternal.h"
#include "Carbon/Overlay/OverlayInternal.h"
#include "Carbon/Renderer/RenderStateInternal.h"
#include "Carbon/Style/StyleInternal.h"
#include "Carbon/Widgets/Internal/TextEditor.h"

namespace Carbon
{
    namespace Internal
    {
        class TextSystem;
    } // namespace Internal

    /// All state of one Carbon instance. Internal: modules reach it through Internal::GetContext().
    struct Context
    {
        Context();
        ~Context();

        Callbacks HostCallbacks;
        IO HostIO;
        Internal::InputState Input;

        /// The ID scopes; the first entry is the root scope and is never popped.
        std::vector<ID> IDStack;
        DrawList Draw;
        std::unique_ptr<Internal::TextSystem> Text;
        /// The renderer backend, if any, and what Carbon tracks for it.
        Internal::RenderState Render;

        Internal::StateStorage States;
        Internal::StyleState Style;
        Internal::LayoutState Layout;
        Internal::InteractionState Interaction;
        Internal::OverlayState Overlays;
        Internal::TextEditState TextEdit;

        /// Reused by widgets that have to compose a string, so steady-state frames do not allocate.
        std::string ScratchText;

        bool ReduceMotion = false;
        /// Set during a frame by anything that is still moving; becomes WasAnimatingLastFrame at EndFrame.
        bool IsAnimatingThisFrame = false;
        bool WasAnimatingLastFrame = false;
        /// The earliest RequestFrameAfter of the frame, in seconds; becomes NextFrameDelayLastFrame at EndFrame.
        float NextFrameDelayThisFrame = std::numeric_limits<float>::infinity();
        float NextFrameDelayLastFrame = std::numeric_limits<float>::infinity();

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
