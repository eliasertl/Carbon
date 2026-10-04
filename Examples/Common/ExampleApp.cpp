#include "ExampleApp.h"

#include <cstdio>

#include "GlfwInput.h"

namespace Example
{
    App::App(int argc, char** argv, const char* title, int width, int height)
        : m_Host(ParseArguments(argc, argv), title, width, height)
    {
        if (!m_Host.IsReady())
            return;

        Carbon::ContextDescription description;
        description.Device = m_Host.GetDevice();
        description.ColorFormat = m_Host.GetColorFormat();
        description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message)
        {
            // Carbon never prints on its own; the host decides. The examples show warnings and errors.
            if (level < Carbon::LogLevel::Warning)
                return;
            const std::string_view name = Carbon::ToString(level);
            std::fprintf(stderr, "[%.*s] %.*s: %.*s\n", static_cast<int>(name.size()), name.data(),
                         static_cast<int>(source.size()), source.data(), static_cast<int>(message.size()),
                         message.data());
        };
        // A failed check was already logged as Fatal above. The examples carry on instead of breaking into a
        // debugger, which Carbon would do in debug builds when no handler is set.
        description.Callbacks.AssertFailed = [](const Carbon::AssertInfo&) {};
        InstallPlatformCallbacks(m_Host, description.Callbacks);
        m_Context = Carbon::CreateContext(description);
        InstallInputCallbacks(m_Host);

        Carbon::SetTheme(GetArguments().IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());
    }

    App::~App()
    {
        if (m_Context != nullptr)
            Carbon::DestroyContext(m_Context);
    }

    int App::Run(const std::function<void()>& build)
    {
        if (!IsReady())
            return 1;

        while (m_Host.BeginFrame())
        {
            Carbon::IO& io = Carbon::GetIO();
            io.SetDisplaySize(m_Host.GetWidth(), m_Host.GetHeight());
            io.SetContentScale(m_Host.GetContentScale());
            io.SetDeltaTime(m_Host.GetDeltaTime());

            Carbon::NewFrame();
            build();
            Carbon::EndFrame();

            // The host clears to the theme's background, which glides during a theme switch.
            const Carbon::Color background = Carbon::GetStyleColor(Carbon::StyleColor::Background);
            wgpu::RenderPassColorAttachment colorAttachment;
            colorAttachment.view = m_Host.GetTargetView();
            colorAttachment.loadOp = wgpu::LoadOp::Clear;
            colorAttachment.storeOp = wgpu::StoreOp::Store;
            colorAttachment.clearValue = {background.R, background.G, background.B, 1.0};
            wgpu::RenderPassDescriptor passDescriptor;
            passDescriptor.colorAttachmentCount = 1;
            passDescriptor.colorAttachments = &colorAttachment;

            const wgpu::CommandEncoder encoder = m_Host.GetDevice().CreateCommandEncoder();
            const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
            Carbon::Render(pass);
            pass.End();
            const wgpu::CommandBuffer commands = encoder.Finish();
            m_Host.GetDevice().GetQueue().Submit(1, &commands);
            m_Host.EndFrame();
        }
        return m_Host.IsReady() ? 0 : 1;
    }
} // namespace Example
