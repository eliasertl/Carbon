#pragma once

namespace Carbon
{
    struct Context;
}

namespace Carbon::Internal
{
    /// Called by NewFrame after the input was applied: derives touch mode and the size class for the frame.
    void UpdateAdaptiveState(Context& context);
} // namespace Carbon::Internal
