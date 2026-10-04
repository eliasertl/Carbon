// Minimal integration of Carbon into a host application.
//
// The host owns the window, the WebGPU device and the render pass. Each frame it forwards input to Carbon,
// builds the interface, draws its own content into the pass and then lets Carbon add the interface on top.
// The window and device chores live in Examples/Common/ExampleHost; everything Carbon-specific is in this file.
//
//   MinimalIntegration [--theme light|dark] [--scale <factor>] [--screenshot <file.png>]

#include <cstdio>
#include <string>

#include <Carbon/Carbon.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "ExampleHost.h"
#include "GlfwInput.h"

namespace
{
    using Carbon::Color;
    using Carbon::Rect;
    using Carbon::Vec2;

    // Until Carbon's themes arrive (milestone 4), the example brings its own colors.
    struct Palette
    {
        Color Background;
        Color Label;
        Color SecondaryLabel;
        Color Control;
        Color Separator;
        Color Accent;
    };

    Palette GetPalette(bool isDark)
    {
        if (isDark)
        {
            return Palette{Color::FromHex(0x000000), Color::FromHex(0xFFFFFF), Color::FromHex(0x98989F),
                           Color::FromHex(0x2C2C2E), Color::FromHex(0x38383A), Color::FromHex(0x0A84FF)};
        }
        return Palette{Color::FromHex(0xFFFFFF), Color::FromHex(0x000000), Color::FromHex(0x6E6E73),
                       Color::FromHex(0xE9E9EB), Color::FromHex(0xD1D1D6), Color::FromHex(0x007AFF)};
    }

    // Where the host draws its own content, in points. Carbon frames it and labels it.
    constexpr Rect HostContentRect(448.0f, 124.0f, 280.0f, 232.0f);

    // ---------------------------------------------------------------------------------------------------------
    // The host's own rendering: one triangle with its own pipeline, drawn into the same pass as Carbon.
    // ---------------------------------------------------------------------------------------------------------
    class HostTriangle
    {
    public:
        HostTriangle(const wgpu::Device& device, wgpu::TextureFormat format)
        {
            static const char* const Source = R"(
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
                    return vec4f(input.color, 1.0);
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
        }

        // Draws the triangle into `area` (in points) of the pass.
        void Draw(const wgpu::RenderPassEncoder& pass, const Rect& area, float contentScale) const
        {
            pass.SetPipeline(m_Pipeline);
            pass.SetViewport(area.X * contentScale, area.Y * contentScale, area.Width * contentScale,
                             area.Height * contentScale, 0.0f, 1.0f);
            pass.Draw(3);
        }

    private:
        wgpu::RenderPipeline m_Pipeline;
    };

    // ---------------------------------------------------------------------------------------------------------
    // The interface. Carbon's widgets and layout arrive in later milestones; for now it is drawn directly with
    // the draw list, which is also what custom components use.
    // ---------------------------------------------------------------------------------------------------------
    void DrawCenteredLabel(Carbon::DrawList& drawList, const Rect& rect, std::string_view text,
                           const Carbon::TextSpec& spec, Color color)
    {
        const Vec2 size = Carbon::MeasureText(text, spec);
        drawList.AddText(rect.GetCenter() - size * 0.5f, text, spec, color);
    }

    void BuildInterface(const Palette& palette)
    {
        Carbon::DrawList& drawList = Carbon::GetDrawList();
        const Carbon::TextSpec body = Carbon::GetTextSpec(Carbon::TextStyle::Body);
        const Carbon::TextSpec headline = Carbon::GetTextSpec(Carbon::TextStyle::Headline);
        const Carbon::TextSpec caption = Carbon::GetTextSpec(Carbon::TextStyle::Caption1);
        const float left = 32.0f;

        drawList.AddText(Vec2(left, 28.0f), "Carbon", Carbon::GetTextSpec(Carbon::TextStyle::LargeTitle, true),
                         palette.Label);
        drawList.AddText(Vec2(left, 64.0f), "Immediate-mode UI, rendered into the host's own render pass.", body,
                         palette.SecondaryLabel);

        // Corner smoothing: the same rectangle with circular, default and maximum smoothing.
        drawList.AddText(Vec2(left, 104.0f), "Corner smoothing", headline, palette.Label);
        const float smoothings[3] = {0.0f, Carbon::DefaultCornerSmoothing, 1.0f};
        const char* const smoothingLabels[3] = {"0 (circular)", "0.6 (default)", "1.0"};
        for (int i = 0; i < 3; i++)
        {
            const Rect shape(left + static_cast<float>(i) * 124.0f, 130.0f, 108.0f, 72.0f);
            drawList.AddSquircle(shape, palette.Control, 26.0f, smoothings[i]);
            drawList.AddText(Vec2(shape.X, shape.GetBottom() + 6.0f), smoothingLabels[i], caption,
                             palette.SecondaryLabel);
        }

        // Shapes that the controls of later milestones are made of.
        drawList.AddText(Vec2(left, 244.0f), "Shapes", headline, palette.Label);
        const float controlTop = 270.0f;

        const Rect cancel(left, controlTop, 74.0f, 24.0f);
        drawList.AddSquircle(cancel, palette.Control, 6.0f);
        DrawCenteredLabel(drawList, cancel, "Cancel", body, palette.Label);

        const Rect save(left + 84.0f, controlTop, 62.0f, 24.0f);
        drawList.AddSquircle(save, palette.Accent, 6.0f);
        DrawCenteredLabel(drawList, save, "Save", body, Color::White());

        const Rect focused(left + 160.0f, controlTop, 84.0f, 24.0f);
        drawList.AddSquircle(focused, palette.Control, 6.0f);
        drawList.AddFocusRing(focused, palette.Accent.WithAlpha(0.5f), 6.0f, 3.0f, 0.0f);
        DrawCenteredLabel(drawList, focused, "Focused", body, palette.Label);

        const Rect field(left + 260.0f, controlTop, 110.0f, 24.0f);
        drawList.AddSquircle(field, palette.Background, 6.0f);
        drawList.AddSquircleStroke(field, palette.Separator, 6.0f, 1.0f);
        drawList.AddText(Vec2(field.X + 8.0f, field.Y + 4.0f), "Text field", body, palette.SecondaryLabel);

        // A switch and a slider.
        const float secondRow = controlTop + 40.0f;
        const Rect switchTrack(left, secondRow, 38.0f, 22.0f);
        drawList.AddSquircle(switchTrack, palette.Accent, 11.0f);
        drawList.AddShadow(Rect(switchTrack.GetRight() - 20.0f, secondRow + 2.0f, 18.0f, 18.0f),
                           Color::Black().WithAlpha(0.25f), 9.0f, 2.0f, Vec2(0.0f, 1.0f));
        drawList.AddCircle(Vec2(switchTrack.GetRight() - 11.0f, secondRow + 11.0f), 9.0f, Color::White());

        const float sliderY = secondRow + 11.0f;
        const float sliderStart = left + 60.0f;
        const float sliderEnd = left + 250.0f;
        const float sliderValue = sliderStart + (sliderEnd - sliderStart) * 0.62f;
        drawList.AddLine(Vec2(sliderStart, sliderY), Vec2(sliderEnd, sliderY), palette.Control, 4.0f);
        drawList.AddLine(Vec2(sliderStart, sliderY), Vec2(sliderValue, sliderY), palette.Accent, 4.0f);
        drawList.AddShadow(Rect::FromCenter(Vec2(sliderValue, sliderY), Vec2(20.0f)), Color::Black().WithAlpha(0.25f),
                           10.0f, 3.0f, Vec2(0.0f, 1.0f));
        drawList.AddCircle(Vec2(sliderValue, sliderY), 10.0f, Color::White());
        drawList.AddCircleStroke(Vec2(sliderValue, sliderY), 10.0f, Color::Black().WithAlpha(0.08f), 0.5f);

        // Icons come from the embedded Phosphor fonts and are drawn through the text pipeline.
        drawList.AddText(Vec2(left, 356.0f), "Icons", headline, palette.Label);
        const std::string icons = std::string(Carbon::Icons::House) + "  " + Carbon::Icons::Gear + "  " +
                                  Carbon::Icons::MagnifyingGlass + "  " + Carbon::Icons::Bell + "  " +
                                  Carbon::Icons::Heart + "  " + Carbon::Icons::Star + "  " + Carbon::Icons::Folder;
        Carbon::TextSpec iconSpec;
        iconSpec.Size = 17.0f;
        iconSpec.LineHeight = 22.0f;
        drawList.AddText(Vec2(left, 380.0f), icons, iconSpec, palette.Label);
        iconSpec.Weight = Carbon::FontWeight::Bold;
        drawList.AddText(Vec2(left, 408.0f), icons, iconSpec, palette.Label);
        iconSpec.Icons = Carbon::IconVariant::Fill;
        drawList.AddText(Vec2(left, 436.0f), icons, iconSpec, palette.Accent);

        // Text and icons share a line; the icon is centered on the capital letters.
        const std::string documents = std::string(Carbon::Icons::Folder) + "  Documents";
        drawList.AddText(Vec2(left + 236.0f, 382.0f), documents, body, palette.Label);
        Carbon::TextSpec italic = body;
        italic.Italic = true;
        drawList.AddText(Vec2(left + 236.0f, 410.0f), "Italic, kerned: AV To Wa", italic, palette.Label);
        Carbon::TextSpec medium = body;
        medium.Weight = Carbon::FontWeight::Medium;
        drawList.AddText(Vec2(left + 236.0f, 438.0f), "Weights from the variable font", medium, palette.Label);

        // A frame around the area the host draws itself.
        drawList.AddText(Vec2(HostContentRect.X, 104.0f), "Host content", headline, palette.Label);
        drawList.AddSquircleStroke(HostContentRect, palette.Separator, 14.0f, 1.0f);
        const Vec2 noteSize = Carbon::MeasureText("Drawn by the host in the same pass", caption);
        drawList.AddText(Vec2(HostContentRect.GetCenter().X - noteSize.X * 0.5f, HostContentRect.GetBottom() - 22.0f),
                         "Drawn by the host in the same pass", caption, palette.SecondaryLabel);
    }
} // namespace

int main(int argc, char** argv)
{
    const Example::Arguments arguments = Example::ParseArguments(argc, argv);
    Example::Host host(arguments, "Carbon - Minimal Integration", 760, 480);
    if (!host.IsReady())
        return 1;

    // ---- 1. Create the Carbon context on the host's device -------------------------------------------------
    Carbon::ContextDescription description;
    description.Device = host.GetDevice();
    description.ColorFormat = host.GetColorFormat();
    description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message)
    {
        const std::string_view levelName = Carbon::ToString(level);
        std::fprintf(stderr, "[%.*s] %.*s: %.*s\n", static_cast<int>(levelName.size()), levelName.data(),
                     static_cast<int>(source.size()), source.data(), static_cast<int>(message.size()), message.data());
    };
    Example::InstallPlatformCallbacks(host, description.Callbacks); // clipboard and cursor, through GLFW
    Carbon::Context* context = Carbon::CreateContext(description);

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

    const Palette palette = GetPalette(arguments.IsDark);
    const HostTriangle triangle(host.GetDevice(), host.GetColorFormat());

    // ---- 3. The frame loop ----------------------------------------------------------------------------------
    while (host.BeginFrame())
    {
        // Display metrics and timing, every frame, before NewFrame.
        Carbon::IO& io = Carbon::GetIO();
        io.SetDisplaySize(host.GetWidth(), host.GetHeight());
        io.SetContentScale(host.GetContentScale());
        io.SetDeltaTime(host.GetDeltaTime());

        Carbon::NewFrame();
        BuildInterface(palette);
        Carbon::EndFrame();

        // The render pass belongs to the host. It clears the target, draws its own content, then Carbon's.
        wgpu::RenderPassColorAttachment colorAttachment;
        colorAttachment.view = host.GetTargetView();
        colorAttachment.loadOp = wgpu::LoadOp::Clear;
        colorAttachment.storeOp = wgpu::StoreOp::Store;
        colorAttachment.clearValue = {palette.Background.R, palette.Background.G, palette.Background.B, 1.0};
        wgpu::RenderPassDescriptor passDescriptor;
        passDescriptor.colorAttachmentCount = 1;
        passDescriptor.colorAttachments = &colorAttachment;

        const wgpu::CommandEncoder encoder = host.GetDevice().CreateCommandEncoder();
        const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
        triangle.Draw(pass, HostContentRect.Inset(Carbon::EdgeInsets(24.0f, 20.0f, 24.0f, 36.0f)),
                      host.GetContentScale());
        Carbon::Render(pass);
        pass.End();

        const wgpu::CommandBuffer commands = encoder.Finish();
        host.GetDevice().GetQueue().Submit(1, &commands);
        host.EndFrame();
    }

    Carbon::DestroyContext(context);
    return host.IsReady() ? 0 : 1;
}
