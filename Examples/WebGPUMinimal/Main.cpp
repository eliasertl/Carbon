// Minimal integration of Carbon into a host application that renders with WebGPU (Dawn).
//
// The host owns the window, the WebGPU device and the render pass. Each frame it forwards input to Carbon,
// builds the interface, draws its own content into the pass and then lets Carbon add the interface on top.
// The window and device chores live in Examples/Common (ExampleHost, Devices/WebGPUDevice); everything
// Carbon-specific is in this file.
// VulkanMinimal and OpenGLMinimal do the same with the other backends.
//
//   WebGPUMinimal [--theme light|dark] [--scale <factor>] [--size <w>x<h>] [--screenshot <file.png>]

#include <cstdio>
#include <string>

#include <Carbon/Backends/WebGPU/WebGPUBackend.h>
#include <Carbon/Carbon.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "ExampleHost.h"
#include "GlfwInput.h"
#include "WebGPUDevice.h"

namespace
{
    // ---------------------------------------------------------------------------------------------------------
    // The host's own rendering: one triangle with its own pipeline, drawn into the same pass as Carbon.
    // ---------------------------------------------------------------------------------------------------------
    class HostTriangle
    {
    public:
        HostTriangle(const wgpu::Device& device, wgpu::TextureFormat format)
        {
            static const char* const Source = R"(
                struct Uniforms {
                    brightness: f32,
                }
                @group(0) @binding(0) var<uniform> uniforms: Uniforms;

                struct VertexOutput {
                    @builtin(position) position: vec4f,
                    @location(0) color: vec3f,
                }

                @vertex
                fn VertexMain(@builtin(vertex_index) index: u32) -> VertexOutput {
                    var positions = array<vec2f, 3>(vec2f(0.0, 0.62), vec2f(-0.68, -0.58), vec2f(0.68, -0.58));
                    var colors = array<vec3f, 3>(vec3f(1.0, 0.27, 0.23), vec3f(0.19, 0.82, 0.35), vec3f(0.04, 0.52, 1.0));
                    var output: VertexOutput;
                    output.position = vec4f(positions[index], 0.0, 1.0);
                    output.color = colors[index];
                    return output;
                }

                @fragment
                fn FragmentMain(input: VertexOutput) -> @location(0) vec4f {
                    return vec4f(input.color * uniforms.brightness, 1.0);
                }
            )";

            wgpu::ShaderSourceWGSL wgsl;
            wgsl.code = Source;
            wgpu::ShaderModuleDescriptor shaderDescriptor;
            shaderDescriptor.nextInChain = &wgsl;
            const wgpu::ShaderModule shader = device.CreateShaderModule(&shaderDescriptor);

            wgpu::ColorTargetState target;
            target.format = format;
            wgpu::FragmentState fragment;
            fragment.module = shader;
            fragment.entryPoint = "FragmentMain";
            fragment.targetCount = 1;
            fragment.targets = &target;

            wgpu::RenderPipelineDescriptor descriptor;
            descriptor.vertex.module = shader;
            descriptor.vertex.entryPoint = "VertexMain";
            descriptor.fragment = &fragment;
            m_Pipeline = device.CreateRenderPipeline(&descriptor);

            wgpu::BufferDescriptor bufferDescriptor;
            bufferDescriptor.size = 16;
            bufferDescriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
            m_Uniforms = device.CreateBuffer(&bufferDescriptor);

            wgpu::BindGroupEntry entry;
            entry.binding = 0;
            entry.buffer = m_Uniforms;
            entry.size = 16;
            wgpu::BindGroupDescriptor groupDescriptor;
            groupDescriptor.layout = m_Pipeline.GetBindGroupLayout(0);
            groupDescriptor.entryCount = 1;
            groupDescriptor.entries = &entry;
            m_BindGroup = device.CreateBindGroup(&groupDescriptor);
            m_Queue = device.GetQueue();
        }

        // Draws the triangle into `area` (in points) of the pass.
        void Draw(const wgpu::RenderPassEncoder& pass, const Carbon::Rect& area, float contentScale,
                  float brightness) const
        {
            if (area.IsEmpty())
                return;
            const float uniforms[4] = {brightness, 0.0f, 0.0f, 0.0f};
            m_Queue.WriteBuffer(m_Uniforms, 0, uniforms, sizeof(uniforms));
            pass.SetPipeline(m_Pipeline);
            pass.SetBindGroup(0, m_BindGroup);
            pass.SetViewport(area.X * contentScale, area.Y * contentScale, area.Width * contentScale,
                             area.Height * contentScale, 0.0f, 1.0f);
            pass.Draw(3);
        }

    private:
        wgpu::RenderPipeline m_Pipeline;
        wgpu::Buffer m_Uniforms;
        wgpu::BindGroup m_BindGroup;
        wgpu::Queue m_Queue;
    };

    // ---------------------------------------------------------------------------------------------------------
    // The interface. It is rebuilt from this code every frame; the application owns all of its state.
    // ---------------------------------------------------------------------------------------------------------
    struct Settings
    {
        bool IsDark = false;
        bool ShowTriangle = true;
        float Brightness = 1.0f;
        std::string Name = "Triangle";
        int Saves = 0;
        // Where the host draws its own content this frame, in points. The interface reserves the space.
        Carbon::Rect HostArea;
    };

    void BuildInterface(Settings& settings)
    {
        using namespace Carbon;

        BeginHStack({.Spacing = 24.0f,
                     .Padding = 24.0f,
                     .Alignment = VerticalAlignment::Top,
                     .Width = Size::Fill(),
                     .Height = Size::Fill()});

        // Left: a column of controls.
        BeginVStack({.Spacing = 12.0f, .Width = 300.0f, .Height = Size::Fill()});
        Text("Settings", {.Style = TextStyle::LargeTitle, .Emphasized = true});
        Text("Carbon's interface and the host's triangle are drawn into the same render pass.",
             {.Secondary = true, .Width = Size::Fill(), .Wraps = true});
        Spacer({.Length = 4.0f});

        if (Toggle("Dark Mode", &settings.IsDark, {.Width = Size::Fill()}))
            SetTheme(settings.IsDark ? Theme::Dark() : Theme::Light()); // animated
        Separator();
        Toggle("Show Triangle", &settings.ShowTriangle, {.Width = Size::Fill()});
        Separator();

        BeginHStack({.Spacing = 10.0f, .Width = Size::Fill()});
        Text("Brightness");
        Slider("Brightness", &settings.Brightness, 0.2f, 1.0f,
               {.Width = Size::Fill(), .Disabled = !settings.ShowTriangle});
        EndHStack();

        BeginHStack({.Spacing = 10.0f, .Width = Size::Fill()});
        Text("Name");
        TextField("Name", &settings.Name, {.Width = Size::Fill()});
        EndHStack();

        Spacer();
        BeginHStack({.Spacing = 8.0f, .Width = Size::Fill()});
        Text(settings.Saves == 0 ? std::string("Not saved yet") : "Saved " + std::to_string(settings.Saves) + " times",
             {.Style = TextStyle::Subheadline, .Secondary = true});
        Spacer();
        if (Button("Reset"))
        {
            settings.Brightness = 1.0f;
            settings.Name = "Triangle";
        }
        if (Button("Save", {.Role = ButtonRole::Prominent, .IsDefault = true}))
            settings.Saves++;
        EndHStack();
        EndVStack();

        // Right: a framed area the interface only reserves; the host fills it after EndFrame.
        BeginVStack({.Spacing = 8.0f, .Width = Size::Fill(), .Height = Size::Fill()});
        Text(std::string(Icons::Cube) + "  " + settings.Name, {.Style = TextStyle::Headline});
        const Rect frame = AllocateItem(Vec2(), {.Width = Size::Fill(), .Height = Size::Fill()});
        GetDrawList().AddSquircleStroke(frame, GetStyleColor(StyleColor::Separator), 14.0f, 1.0f);
        settings.HostArea = settings.ShowTriangle ? frame.Inset(EdgeInsets(24.0f)) : Rect();
        Text("Drawn by the host in the same pass", {.Style = TextStyle::Caption1, .Secondary = true});
        EndVStack();

        EndHStack();
    }
} // namespace

int main(int argc, char** argv)
{
    const Example::Arguments arguments = Example::ParseArguments(argc, argv);
    Example::Host host(arguments, "Carbon - WebGPU Minimal Integration", 760, 440);
    if (!host.IsReady())
        return 1;
    // This executable links the WebGPU device of Examples/Common.
    const Example::WebGPUDevice& gpu = static_cast<const Example::WebGPUDevice&>(host.GetDevice());

    // ---- 1. Create the Carbon context and install the WebGPU backend on the host's device ---------------------
    Carbon::ContextDescription description;
    description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message)
    {
        const std::string_view levelName = Carbon::ToString(level);
        std::fprintf(stderr, "[%.*s] %.*s: %.*s\n", static_cast<int>(levelName.size()), levelName.data(),
                     static_cast<int>(source.size()), source.data(), static_cast<int>(message.size()), message.data());
    };
    Example::InstallPlatformCallbacks(host.GetWindow(), description.Callbacks); // clipboard and cursor
    Carbon::Context* context = Carbon::CreateContext(description);

    Carbon::WebGPUInitInfo info;
    info.Device = gpu.GetDevice();
    info.ColorFormat = gpu.GetCarbonColorFormat(); // the format of the passes Carbon will draw into
    if (!Carbon::WebGPUInit(info))
        return 1;

    // The host chooses the appearance; Carbon cannot detect the OS setting.
    Settings settings;
    settings.IsDark = arguments.IsDark;
    Carbon::SetTheme(settings.IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());

    // ---- 2. Forward the window's input events to Carbon ------------------------------------------------------
    // Written out here because it is the heart of an integration; the other examples call
    // Example::InstallInputCallbacks, which does the same.
    if (GLFWwindow* window = host.GetWindow())
    {
        glfwSetCursorPosCallback(window,
                                 [](GLFWwindow* source, double cursorX, double cursorY)
                                 {
                                     const Example::Host* owner =
                                         static_cast<const Example::Host*>(glfwGetWindowUserPointer(source));
                                     float x = 0.0f;
                                     float y = 0.0f;
                                     owner->CursorToPoints(cursorX, cursorY, x, y); // Carbon works in points
                                     Carbon::GetIO().AddMousePosEvent(x, y);
                                 });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow*, int entered)
                                   {
                                       if (entered == GLFW_FALSE)
                                           Carbon::GetIO().AddMouseLeaveEvent();
                                   });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow*, int glfwButton, int action, int)
                                   {
                                       Carbon::MouseButton button;
                                       if (Example::ToCarbonMouseButton(glfwButton, button))
                                           Carbon::GetIO().AddMouseButtonEvent(button, action != GLFW_RELEASE);
                                   });
        glfwSetScrollCallback(
            window, [](GLFWwindow*, double scrollX, double scrollY)
            { Carbon::GetIO().AddMouseWheelEvent(static_cast<float>(scrollX), static_cast<float>(scrollY)); });
        glfwSetKeyCallback(window,
                           [](GLFWwindow*, int glfwKey, int, int action, int)
                           {
                               if (action != GLFW_REPEAT) // Carbon repeats held keys itself
                                   Carbon::GetIO().AddKeyEvent(Example::ToCarbonKey(glfwKey), action == GLFW_PRESS);
                           });
        glfwSetCharCallback(window, [](GLFWwindow*, unsigned int codepoint)
                            { Carbon::GetIO().AddInputCharacter(static_cast<char32_t>(codepoint)); });
        glfwSetWindowFocusCallback(
            window, [](GLFWwindow*, int focused) { Carbon::GetIO().AddFocusEvent(focused == GLFW_TRUE); });
    }

    const HostTriangle triangle(gpu.GetDevice(), gpu.GetColorFormat());

    // ---- 3. The frame loop ----------------------------------------------------------------------------------
    while (host.BeginFrame())
    {
        // Display metrics and timing, every frame, before NewFrame.
        Carbon::IO& io = Carbon::GetIO();
        io.SetDisplaySize(host.GetWidth(), host.GetHeight());
        io.SetContentScale(host.GetContentScale());
        io.SetDeltaTime(host.GetDeltaTime());

        Carbon::NewFrame();
        BuildInterface(settings);
        Carbon::EndFrame();

        // The render pass belongs to the host. It clears the target, draws its own content, then Carbon's.
        const Carbon::Color background = Carbon::GetStyleColor(Carbon::StyleColor::Background);
        wgpu::RenderPassColorAttachment colorAttachment;
        colorAttachment.view = gpu.GetTargetView();
        colorAttachment.loadOp = wgpu::LoadOp::Clear;
        colorAttachment.storeOp = wgpu::StoreOp::Store;
        colorAttachment.clearValue = {background.R, background.G, background.B, 1.0};
        wgpu::RenderPassDescriptor passDescriptor;
        passDescriptor.colorAttachmentCount = 1;
        passDescriptor.colorAttachments = &colorAttachment;

        const wgpu::CommandEncoder encoder = gpu.GetDevice().CreateCommandEncoder();
        const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
        triangle.Draw(pass, settings.HostArea, host.GetContentScale(), settings.Brightness);
        Carbon::WebGPURender(pass);
        pass.End();

        const wgpu::CommandBuffer commands = encoder.Finish();
        gpu.GetDevice().GetQueue().Submit(1, &commands);
        host.EndFrame();
    }

    Carbon::WebGPUShutdown();
    Carbon::DestroyContext(context);
    return host.IsReady() ? 0 : 1;
}
