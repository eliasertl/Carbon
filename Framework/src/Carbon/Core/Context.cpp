#include "Carbon/Core/Context.h"

#include <cstdlib>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextDescription.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Core/Version.h"

namespace Carbon
{
    namespace Internal
    {
        Context* g_CurrentContext = nullptr;

        Context& GetContext()
        {
            // Without a context there is nothing to recover to, so this is fatal in every build type.
            if (g_CurrentContext == nullptr) [[unlikely]]
            {
                CB_DEBUG_BREAK();
                std::abort();
            }
            return *g_CurrentContext;
        }

        Context& GetFrameContext()
        {
            Context& context = GetContext();
            CB_VERIFY(context.IsInFrame, "This function must be called between NewFrame and EndFrame");
            return context;
        }
    } // namespace Internal

    Context* CreateContext(const ContextDescription& description)
    {
        Context* context = new Context();
        context->HostCallbacks = description.Callbacks;
        context->IDStack.push_back(HashID("Carbon"));

        Context* previous = Internal::g_CurrentContext;
        if (previous == nullptr)
            Internal::g_CurrentContext = context;

        if (context->HostCallbacks.Log)
        {
            context->HostCallbacks.Log(LogLevel::Info, "Core",
                                       std::format("Carbon {} context created{}", GetVersionString(),
                                                   description.Device ? "" : " (headless: no device)"));
        }
        return context;
    }

    void DestroyContext(Context* context)
    {
        if (context == nullptr)
            context = Internal::g_CurrentContext;
        if (context == nullptr)
            return;
        if (Internal::g_CurrentContext == context)
            Internal::g_CurrentContext = nullptr;
        delete context;
    }

    void SetCurrentContext(Context* context)
    {
        Internal::g_CurrentContext = context;
    }

    Context* GetCurrentContext()
    {
        return Internal::g_CurrentContext;
    }

    void NewFrame()
    {
        Context& context = Internal::GetContext();
        CB_VERIFY(!context.IsInFrame, "NewFrame called twice without EndFrame");

        IO& io = context.HostIO;
        context.DisplaySize = io.GetDisplaySize();
        context.Scale.Factor = io.GetContentScale();
        context.DeltaTime = io.GetDeltaTime();
        context.Time += context.DeltaTime;
        context.FrameCount++;

        context.Input.Update(io, context.Time);

        context.IDStack.resize(1);
        context.Draw.Reset(Rect(Vec2(), context.DisplaySize), context.Scale.Factor);
        context.IsInFrame = true;
    }

    void EndFrame()
    {
        Context& context = Internal::GetContext();
        CB_VERIFY(context.IsInFrame, "EndFrame called without NewFrame");
        if (!context.IsInFrame)
            return;

        CB_VERIFY(context.IDStack.size() == 1, "Unbalanced ID stack: {} PushID call(s) without PopID",
                  context.IDStack.size() - 1);
        context.IDStack.resize(1);

        CB_VERIFY(context.Draw.IsBalanced(),
                  "Unbalanced draw list: a PushLayer, PushClipRect or PushOpacity was not popped");

        context.Draw.Finalize();
        context.IsInFrame = false;
    }

    const DrawData& GetDrawData()
    {
        return Internal::GetContext().Draw.GetDrawData();
    }

    uint64_t GetFrameCount()
    {
        return Internal::GetContext().FrameCount;
    }

    double GetTime()
    {
        return Internal::GetContext().Time;
    }
} // namespace Carbon
