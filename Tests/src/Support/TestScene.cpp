#include "Support/TestScene.h"

#include <cmath>
#include <format>

namespace Carbon
{
    namespace
    {
        // Screenshots of the examples settle the same way: a few long frames finish every animation.
        constexpr int WarmupFrames = 12;
        constexpr float WarmupDeltaTime = 0.25f;
    } // namespace

    std::vector<uint8_t> TestScene::GetImageTexels()
    {
        std::vector<uint8_t> texels(static_cast<size_t>(ImageSize) * ImageSize * 4);
        for (uint32_t y = 0; y < ImageSize; y++)
        {
            for (uint32_t x = 0; x < ImageSize; x++)
            {
                uint8_t* texel = &texels[(static_cast<size_t>(y) * ImageSize + x) * 4];
                const bool right = x >= ImageSize / 2;
                const bool bottom = y >= ImageSize / 2;
                const Color color = x == y   ? Color::White()
                                    : bottom ? (right ? Color::FromHex(0xFFCC00) : Color::FromHex(0x0A84FF))
                                             : (right ? Color::FromHex(0x34C759) : Color::FromHex(0xFF3B30));
                const uint32_t packed = color.ToRGBA8();
                texel[0] = static_cast<uint8_t>(packed & 0xFF);
                texel[1] = static_cast<uint8_t>((packed >> 8) & 0xFF);
                texel[2] = static_cast<uint8_t>((packed >> 16) & 0xFF);
                texel[3] = 255;
            }
        }
        return texels;
    }

    void TestScene::Build(TextureID image)
    {
        BeginVStack({.Spacing = 10.0f, .Padding = 16.0f, .Width = Size::Fill(), .Height = Size::Fill()});
        Text("Backends", {.Style = TextStyle::Title2, .Emphasized = true});
        Text(std::string(Icons::Gear) + " Squircles, text and " + Icons::Image + " images",
             {.Style = TextStyle::Body, .Secondary = true});

        BeginHStack({.Spacing = 8.0f, .Alignment = VerticalAlignment::Center});
        Button("Cancel");
        Button("Save", {.Role = ButtonRole::Prominent});
        Toggle("##switch", &m_IsOn);
        Toggle("Check", &m_IsChecked, {.Kind = ToggleKind::Checkbox});
        EndHStack();

        BeginHStack({.Spacing = 8.0f, .Alignment = VerticalAlignment::Center, .Width = Size::Fill()});
        Slider("Value", &m_Value, 0.0f, 1.0f, {.Width = Size::Fixed(120.0f)});
        TextField("Name", &m_Name, {.Width = Size::Fill()});
        EndHStack();
        Separator();

        BeginHStack({.Spacing = 12.0f, .Alignment = VerticalAlignment::Center});
        Image(image, Vec2(56.0f, 56.0f), {.CornerRadius = 12.0f});
        const Rect shapes = AllocateItem(Vec2(240.0f, 64.0f));
        EndHStack();
        EndVStack();

        // Shapes the controls do not cover: shadows, strokes of several widths, circles, lines, clipped text.
        DrawList& drawList = GetDrawList();
        const Color label = GetStyleColor(StyleColor::Label);
        const Color accent = GetStyleColor(StyleColor::Accent);
        const Rect card(shapes.X + 4.0f, shapes.Y + 6.0f, 64.0f, 48.0f);
        drawList.AddShadow(card, GetStyleColor(StyleColor::Shadow), 10.0f, 8.0f, Vec2(0.0f, 3.0f));
        drawList.AddSquircle(card, GetStyleColor(StyleColor::SecondaryBackground), 10.0f);
        drawList.AddSquircleStroke(card, accent, 10.0f, 1.5f);
        drawList.AddCircle(Vec2(shapes.X + 96.0f, shapes.GetCenter().Y), 14.0f, GetStyleColor(StyleColor::Red));
        drawList.AddCircleStroke(Vec2(shapes.X + 96.0f, shapes.GetCenter().Y), 20.0f, label, 1.0f);
        drawList.AddLine(Vec2(shapes.X + 124.0f, shapes.Y + 8.0f), Vec2(shapes.X + 150.0f, shapes.GetBottom() - 8.0f),
                         GetStyleColor(StyleColor::Green), 3.0f);

        const Rect clip(shapes.X + 160.0f, shapes.Y + 10.0f, 72.0f, 44.0f);
        drawList.AddSquircleStroke(clip, label.WithAlpha(0.3f), 0.0f, 1.0f, 0.0f);
        drawList.PushClipRect(clip);
        TextSpec spec = GetTextSpec(TextStyle::Title1);
        drawList.AddText(Vec2(clip.X - 12.0f, clip.Y + 4.0f), "Clipped", spec, accent);
        drawList.PopClipRect();
    }

    RenderedImage RenderTestScene(BackendHarness& harness, bool isDark, float contentScale,
                                  std::vector<std::string>& problems)
    {
        ContextDescription description;
        description.Callbacks.Log = [&problems](LogLevel level, std::string_view source, std::string_view message)
        {
            if (level >= LogLevel::Warning)
                problems.push_back(std::format("[{}] {}: {}", ToString(level), source, message));
        };
        description.Callbacks.AssertFailed = [&problems](const AssertInfo& info)
        { problems.push_back(std::format("[Check] {}", info.Message)); };
        Context* previous = GetCurrentContext();
        Context* context = CreateContext(description);
        SetCurrentContext(context);

        RenderedImage image;
        if (harness.InitBackend(TextureFormat::RGBA8Unorm))
        {
            SetTheme(isDark ? Theme::Dark() : Theme::Light());
            const size_t texture =
                harness.CreateTexture(TestScene::ImageSize, TestScene::ImageSize, TestScene::GetImageTexels());
            TestScene scene;
            IO& io = GetIO();
            io.SetDisplaySize(TestScene::Width, TestScene::Height);
            io.SetContentScale(contentScale);
            for (int frame = 0; frame < WarmupFrames; frame++)
            {
                io.SetDeltaTime(WarmupDeltaTime);
                NewFrame();
                scene.Build(harness.GetTextureID(texture));
                EndFrame();
            }
            const Color background = GetStyleColor(StyleColor::Background);
            image =
                harness.RenderFrame(static_cast<uint32_t>(std::lround(TestScene::Width * contentScale)),
                                    static_cast<uint32_t>(std::lround(TestScene::Height * contentScale)), background);
            harness.ShutdownBackend();
        }
        else
        {
            problems.push_back(std::format("Could not initialize the {} backend", harness.GetName()));
        }

        DestroyContext(context);
        SetCurrentContext(previous);
        for (std::string& message : harness.TakeMessages())
            problems.push_back(std::format("[{}] {}", harness.GetName(), message));
        return image;
    }
} // namespace Carbon
