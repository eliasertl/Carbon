#pragma once

#include <cstdint>

#include "Carbon/Core/ContentScale.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Draw/DrawTypes.h"

namespace Carbon
{
    struct Context;
    struct ContextDescription;

    /// Creates a context. It becomes the current context if there is none.
    Context* CreateContext(const ContextDescription& description);
    /// Destroys a context; null destroys the current one. Destroying the current context leaves none current.
    void DestroyContext(Context* context = nullptr);
    /// Makes a context the target of all Carbon calls. Carbon has one current context at a time.
    void SetCurrentContext(Context* context);
    /// Returns the current context, or null.
    Context* GetCurrentContext();

    /// Starts a frame: applies the input queued on the IO object and resets per-frame state. Set the display
    /// size, content scale and delta time on the IO object before calling it.
    void NewFrame();
    /// Ends the frame: finishes layout and produces the draw data. No widget calls are allowed until NewFrame.
    void EndFrame();

    /// The draw data of the last finished frame. Valid from EndFrame until the next NewFrame.
    const DrawData& GetDrawData();

    /// Number of frames started on the current context.
    uint64_t GetFrameCount();
    /// Seconds accumulated from the delta times of all frames.
    double GetTime();
    /// Seconds the current frame advances time by, as set on the IO object.
    float GetDeltaTime();
    /// Size of the area Carbon draws into, in points.
    Vec2 GetDisplaySize();
    /// Pixels per point of the current frame. Use its Snap functions to put edges on whole pixels.
    ContentScale GetContentScale();
} // namespace Carbon
