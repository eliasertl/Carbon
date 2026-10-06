#include "Carbon/Core/Context.h"

#include <cstdlib>

#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextDescription.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/Log.h"
#include "Carbon/Core/Version.h"
#include "Carbon/Renderer/RendererInternal.h"
#include "Carbon/Text/Internal/TextSystem.h"

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

    Context::Context() = default;
    Context::~Context() = default;

    Context* CreateContext(const ContextDescription& description)
    {
        Context* context = new Context();
        context->HostCallbacks = description.Callbacks;
        context->IDStack.push_back(HashID("Carbon"));
        context->Text = std::make_unique<Internal::TextSystem>();

        if (context->HostCallbacks.Log)
        {
            context->HostCallbacks.Log(LogLevel::Info, "Core",
                                       std::format("Carbon {} context created", GetVersionString()));
        }

        // Until the WebGPU backend has its own Init function, a device in the description installs it.
        if (description.Device != nullptr)
        {
            Context* current = Internal::g_CurrentContext;
            Internal::g_CurrentContext = context;
            Internal::InstallRenderer(*context, description.Device, description.ColorFormat,
                                      description.DepthStencilFormat, description.SampleCount);
            Internal::g_CurrentContext = current;
        }

        if (Internal::g_CurrentContext == nullptr)
            Internal::g_CurrentContext = context;
        return context;
    }

    void DestroyContext(Context* context)
    {
        if (context == nullptr)
            context = Internal::g_CurrentContext;
        if (context == nullptr)
            return;

        // A backend may log or call Carbon while it shuts down, so its context is current meanwhile.
        Context* current = Internal::g_CurrentContext;
        Internal::g_CurrentContext = context;
        Internal::DestroyRendererBackend(*context);
        Internal::g_CurrentContext = current == context ? nullptr : current;
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
        context.Text->BeginFrame(context.FrameCount, context.Scale.Factor);
        Internal::BeginRenderFrame(context);

        context.IsAnimatingThisFrame = context.Style.Advance(context.DeltaTime, context.ReduceMotion);
        context.Style.ResetWorkingValues();

        context.IDStack.resize(1);
        context.Draw.Reset(Rect(Vec2(), context.DisplaySize), context.Scale.Factor);
        Internal::BeginLayout(context);
        Internal::BeginOverlays(context);
        Internal::BeginInteraction(context);
        context.IsInFrame = true;
    }

    void EndFrame()
    {
        Context& context = Internal::GetContext();
        CB_VERIFY(context.IsInFrame, "EndFrame called without NewFrame");
        if (!context.IsInFrame)
            return;

        Internal::EndInteraction(context);
        Internal::EndLayout(context);
        Internal::EndOverlays(context);

        CB_VERIFY(context.IDStack.size() == 1, "Unbalanced ID stack: {} PushID call(s) without PopID",
                  context.IDStack.size() - 1);
        context.IDStack.resize(1);

        CB_VERIFY(context.Style.ColorStack.empty() && context.Style.VarStack.empty(),
                  "Unbalanced style stack: {} PushStyleColor and {} PushStyleVar call(s) without a matching pop",
                  context.Style.ColorStack.size(), context.Style.VarStack.size());
        CB_VERIFY(context.Style.FontStack.empty(), "Unbalanced font stack: {} PushFont call(s) without PopFont",
                  context.Style.FontStack.size());
        context.Style.ResetWorkingValues();

        CB_VERIFY(context.Draw.IsBalanced(),
                  "Unbalanced draw list: a PushLayer, PushClipRect or PushOpacity was not popped");

        context.Draw.Finalize();
        context.States.EndFrame(context.FrameCount);
        context.WasAnimatingLastFrame = context.IsAnimatingThisFrame;
        context.IsInFrame = false;
        Internal::EndRenderFrame(context);
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

    float GetDeltaTime()
    {
        return Internal::GetContext().DeltaTime;
    }

    Vec2 GetDisplaySize()
    {
        return Internal::GetContext().DisplaySize;
    }

    ContentScale GetContentScale()
    {
        return Internal::GetContext().Scale;
    }
} // namespace Carbon
