#include "Support/WidgetTest.h"

#include <cmath>
#include <string>
#include <vector>

#include "Carbon/Core/ContextInternal.h"

namespace Carbon
{
    using WidgetTests = WidgetTest;

    // ---- Text -----------------------------------------------------------------------------------------------

    TEST_F(WidgetTests, TextFitsItsContent)
    {
        Rect rect;
        Settle(
            [&]
            {
                Text("Settings", {.Style = TextStyle::LargeTitle});
                rect = GetItemRect();
            });
        const Vec2 expected = MeasureText("Settings", GetTextSpec(TextStyle::LargeTitle));
        EXPECT_FLOAT_EQ(rect.Width, expected.X);
        EXPECT_FLOAT_EQ(rect.Height, 32.0f); // the style's line height
        // Drawn in the label color.
        EXPECT_EQ(GetQuadColor(0), GetStyleColor(StyleColor::Label));
    }

    TEST_F(WidgetTests, TextColorsAndWeights)
    {
        Settle(
            []
            {
                Text("Secondary", {.Secondary = true});
                Text("Custom", {.Color = Color::FromHex(0xFF3B30)});
            });
        const DrawData& drawData = GetDrawData();
        EXPECT_EQ(GetQuadColor(0), GetStyleColor(StyleColor::SecondaryLabel));
        EXPECT_EQ(GetQuadColor(drawData.Vertices.size() / 4 - 1), Color::FromHex(0xFF3B30));

        // Emphasized and explicit weights change the glyphs, hence the width.
        Frame(
            []
            {
                Text("Weight");
                const float regular = GetItemRect().Width;
                Text("Weight", {.Emphasized = true});
                const float emphasized = GetItemRect().Width;
                Text("Weight", {.Weight = FontWeight::Black});
                const float black = GetItemRect().Width;
                EXPECT_GT(emphasized, regular);
                EXPECT_GT(black, emphasized);
            });
    }

    TEST_F(WidgetTests, TextWrapsInsideAFixedWidth)
    {
        const std::string_view sentence = "The quick brown fox jumps over the lazy dog near the quiet river bank";
        Rect wrapped, truncated, unlimited;
        Settle(
            [&]
            {
                Text(sentence);
                unlimited = GetItemRect();
                Text(sentence, {.Width = 150.0f, .Wraps = true});
                wrapped = GetItemRect();
                Text(sentence, {.Width = 150.0f});
                truncated = GetItemRect();
            });
        EXPECT_GT(unlimited.Width, 300.0f);
        EXPECT_FLOAT_EQ(unlimited.Height, 16.0f);

        // Wrapped: the requested width, several lines tall.
        EXPECT_FLOAT_EQ(wrapped.Width, 150.0f);
        EXPECT_GE(wrapped.Height, 3.0f * 16.0f);
        EXPECT_FLOAT_EQ(std::fmod(wrapped.Height, 16.0f), 0.0f);

        // Not wrapped: one line, cut off with an ellipsis.
        EXPECT_FLOAT_EQ(truncated.Width, 150.0f);
        EXPECT_FLOAT_EQ(truncated.Height, 16.0f);

        // No glyph is drawn outside the box of the wrapped or truncated text.
        for (const DrawVertex& vertex : GetDrawData().Vertices)
        {
            if (vertex.Position.Y >= wrapped.Y - 1.0f)
                EXPECT_LE(vertex.Position.X, 151.5f);
        }
    }

    TEST_F(WidgetTests, IconIsASquare)
    {
        Rect rect;
        Settle(
            [&]
            {
                Icon(Icons::Gear, {.Size = 20.0f});
                rect = GetItemRect();
            });
        EXPECT_EQ(rect.GetSize(), Vec2(20.0f, 20.0f));
        ASSERT_EQ(GetDrawData().Vertices.size(), 4u);
        // The glyph is centered in its square, give or take a pixel of rounding.
        const Vec2 glyphCenter = (GetDrawData().Vertices[0].Position + GetDrawData().Vertices[2].Position) * 0.5f;
        EXPECT_NEAR(glyphCenter.X, rect.GetCenter().X, 1.0f);
        EXPECT_NEAR(glyphCenter.Y, rect.GetCenter().Y, 1.0f);
    }

    // ---- Separator ------------------------------------------------------------------------------------------

    TEST_F(WidgetTests, SeparatorSpansItsStack)
    {
        Rect stack;
        Settle(
            [&]
            {
                BeginVStack({.Spacing = 0.0f, .Padding = 10.0f, .Width = 220.0f});
                AllocateItem(Vec2(50.0f, 20.0f));
                Separator();
                AllocateItem(Vec2(50.0f, 20.0f));
                EndVStack();
                stack = GetLastItemRect();
            });
        ASSERT_EQ(GetDrawData().Vertices.size(), 4u);
        const DrawPrimitive& line = GetPrimitive(0);
        EXPECT_EQ(line.HalfSize, Vec2(100.0f, 0.5f)); // the content width, one point thick
        EXPECT_EQ(GetQuadColor(0), GetStyleColor(StyleColor::Separator));
        EXPECT_FLOAT_EQ(stack.Height, 20.0f + 1.0f + 20.0f + 20.0f);
    }

    TEST_F(WidgetTests, SeparatorIsVerticalInAHorizontalStackAndOnePixelAtLeast)
    {
        GetIO().SetContentScale(2.0f);
        Settle(
            []
            {
                BeginHStack({.Spacing = 0.0f, .Height = 40.0f});
                AllocateItem(Vec2(50.0f, 20.0f));
                Separator({.Thickness = 0.1f});
                EndHStack();
            });
        const DrawPrimitive& line = GetPrimitive(0);
        EXPECT_FLOAT_EQ(line.HalfSize.Y, 20.0f);
        EXPECT_FLOAT_EQ(line.HalfSize.X, 0.25f); // 0.1 pt would vanish; one pixel (0.5 pt at 2x) is the minimum
    }

    // ---- Button ---------------------------------------------------------------------------------------------

    TEST_F(WidgetTests, ButtonSizeFollowsItsLabel)
    {
        Rect regular, small, large, filled;
        Settle(
            [&]
            {
                BeginVStack({.Width = 300.0f});
                Button("Save");
                regular = GetItemRect();
                Button("Save##small", {.ControlSize = ControlSize::Small});
                small = GetItemRect();
                Button("Save##large", {.ControlSize = ControlSize::Large});
                large = GetItemRect();
                Button("Save##fill", {.Width = Size::Fill()});
                filled = GetItemRect();
                EndVStack();
            });
        const float labelWidth = MeasureText("Save", GetTextSpec(TextStyle::Body)).X;
        EXPECT_FLOAT_EQ(regular.Height, 24.0f);
        EXPECT_FLOAT_EQ(regular.Width, labelWidth + 2.0f * GetStyleVar(StyleVar::ControlPadding));
        EXPECT_FLOAT_EQ(small.Height, 20.0f);
        EXPECT_LT(small.Width, regular.Width);
        EXPECT_FLOAT_EQ(large.Height, 30.0f);
        EXPECT_GT(large.Width, regular.Width);
        EXPECT_FLOAT_EQ(filled.Width, 300.0f);
    }

    TEST_F(WidgetTests, ButtonReturnsTrueWhenClicked)
    {
        int saves = 0;
        int cancels = 0;
        Rect save;
        const Builder build = [&]
        {
            BeginHStack();
            cancels += Button("Cancel") ? 1 : 0;
            saves += Button("Save", {.Role = ButtonRole::Prominent}) ? 1 : 0;
            save = GetItemRect();
            EndHStack();
        };
        Settle(build);
        Click(save.GetCenter(), build);
        EXPECT_EQ(saves, 1);
        EXPECT_EQ(cancels, 0);

        // And by keyboard: Tab moves on from the clicked button, wrapping to the first one; Space activates it.
        TapKey(Key::Tab, build);
        TapKey(Key::Space, build);
        EXPECT_EQ(cancels, 1);
    }

    TEST_F(WidgetTests, ButtonRolesUseTheirColors)
    {
        Settle(
            []
            {
                Button("Default");
                Button("Prominent", {.Role = ButtonRole::Prominent});
                Button("Plain", {.Role = ButtonRole::Plain});
                Button("Delete", {.Role = ButtonRole::Destructive});
                Button("Tinted", {.Role = ButtonRole::Prominent, .Tint = Color::FromHex(0x34C759)});
            });

        // Collect the fill and the first glyph color of each button, in order.
        const DrawData& drawData = GetDrawData();
        std::vector<Color> fills;
        std::vector<Color> labels;
        for (size_t quad = 0; quad < drawData.Vertices.size() / 4; quad++)
        {
            const DrawPrimitiveKind kind = GetPrimitive(quad * 4).Kind;
            if (kind == DrawPrimitiveKind::Squircle)
            {
                fills.push_back(GetQuadColor(quad));
                labels.push_back(GetQuadColor(quad + 1));
            }
        }
        // The plain button has no background at rest, so it contributes no squircle.
        ASSERT_EQ(fills.size(), 4u);
        EXPECT_EQ(fills[0], GetStyleColor(StyleColor::ControlFill));
        EXPECT_EQ(labels[0], GetStyleColor(StyleColor::Label));
        EXPECT_EQ(fills[1], GetStyleColor(StyleColor::Accent));
        EXPECT_EQ(labels[1], GetStyleColor(StyleColor::OnAccent));
        EXPECT_EQ(labels[2], GetStyleColor(StyleColor::Destructive));
        EXPECT_EQ(fills[3], Color::FromHex(0x34C759));
    }

    TEST_F(WidgetTests, ButtonShowsHoverAndPressFeedback)
    {
        Rect rect;
        const Builder build = [&]
        {
            Button("Hover me");
            rect = GetItemRect();
        };
        Settle(build);
        const Color idle = GetQuadColor(0);

        MoveMouse(rect.GetCenter(), build);
        Settle(build, 30);
        const Color hovered = GetQuadColor(0);
        EXPECT_LT(hovered.R, idle.R); // shifted towards the (black) label color

        PressMouse(build);
        Settle(build, 30);
        const Color pressed = GetQuadColor(0);
        EXPECT_LT(pressed.R, hovered.R);

        ReleaseMouse(build);
        MoveMouse(Vec2(600.0f, 500.0f), build);
        Settle(build, 30);
        EXPECT_EQ(GetQuadColor(0), idle);
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(WidgetTests, DisabledButtonIsDimmedAndInert)
    {
        int clicks = 0;
        Rect rect;
        const Builder build = [&]
        {
            clicks += Button("Disabled", {.Disabled = true}) ? 1 : 0;
            rect = GetItemRect();
        };
        Settle(build);
        EXPECT_NEAR(GetQuadColor(0).A, GetStyleVar(StyleVar::DisabledOpacity), 0.01f);
        Click(rect.GetCenter(), build);
        EXPECT_EQ(clicks, 0);
        // It is not a Tab stop either.
        TapKey(Key::Tab, build);
        EXPECT_FALSE(GetFocusedID().IsValid());
    }

    TEST_F(WidgetTests, ButtonsWithTheSameLabelNeedDistinctIDs)
    {
        int first = 0;
        int second = 0;
        Rect secondRect;
        const Builder build = [&]
        {
            first += Button("Delete##1") ? 1 : 0;
            second += Button("Delete##2") ? 1 : 0;
            secondRect = GetItemRect();
        };
        Settle(build);
        Click(secondRect.GetCenter(), build);
        EXPECT_EQ(first, 0);
        EXPECT_EQ(second, 1);
        // Both show just "Delete": the same width.
        EXPECT_FLOAT_EQ(secondRect.Width, MeasureText("Delete", GetTextSpec(TextStyle::Body)).X +
                                              2.0f * GetStyleVar(StyleVar::ControlPadding));
    }

    TEST_F(WidgetTests, IconButton)
    {
        Rect iconOnly, withTitle;
        Settle(
            [&]
            {
                Button("##add", {.Icon = Icons::Plus});
                iconOnly = GetItemRect();
                Button("Add", {.Icon = Icons::Plus});
                withTitle = GetItemRect();
            });
        EXPECT_GT(iconOnly.Width, 24.0f);
        EXPECT_LT(iconOnly.Width, 44.0f);
        EXPECT_GT(withTitle.Width, iconOnly.Width + 20.0f);
    }

    // ---- Toggle ---------------------------------------------------------------------------------------------

    TEST_F(WidgetTests, ToggleFlipsOnClickAndSpace)
    {
        bool value = false;
        int changes = 0;
        Rect rect;
        const Builder build = [&]
        {
            changes += Toggle("Dark Mode", &value) ? 1 : 0;
            rect = GetItemRect();
        };
        Settle(build);

        // Clicking the label part of the row works too.
        Click(Vec2(rect.X + 5.0f, rect.GetCenter().Y), build);
        EXPECT_TRUE(value);
        EXPECT_EQ(changes, 1);

        Click(Vec2(rect.GetRight() - 5.0f, rect.GetCenter().Y), build);
        EXPECT_FALSE(value);
        EXPECT_EQ(changes, 2);

        TapKey(Key::Space, build); // still focused from the click
        EXPECT_TRUE(value);
        EXPECT_EQ(changes, 3);
    }

    TEST_F(WidgetTests, SwitchKnobGlidesAndTrackTakesTheAccentColor)
    {
        bool value = false;
        const Builder build = [&] { Toggle("##switch", &value); };
        Settle(build);

        // Quads: track, knob shadow, knob.
        const DrawData& off = GetDrawData();
        ASSERT_EQ(off.Vertices.size(), 12u);
        const float offKnobX = off.Vertices[8].Position.X;
        EXPECT_NE(GetQuadColor(0), GetStyleColor(StyleColor::Accent));
        EXPECT_EQ(GetQuadColor(2), GetStyleColor(StyleColor::Knob));

        value = true;
        Frame(build);
        Frame(build);
        const float midKnobX = GetDrawData().Vertices[8].Position.X;
        EXPECT_GT(midKnobX, offKnobX);
        EXPECT_TRUE(IsAnimating());

        Settle(build, 90);
        const float onKnobX = GetDrawData().Vertices[8].Position.X;
        EXPECT_GT(onKnobX, midKnobX);
        EXPECT_FLOAT_EQ(onKnobX - offKnobX, 38.0f - 18.0f - 4.0f); // track width - knob - both insets
        EXPECT_EQ(GetQuadColor(0), GetStyleColor(StyleColor::Accent));
        EXPECT_FALSE(IsAnimating());
    }

    TEST_F(WidgetTests, StretchedSwitchSitsAtTheTrailingEdge)
    {
        bool value = false;
        Settle(
            [&]
            {
                BeginVStack({.Width = 300.0f});
                Toggle("Wi-Fi", &value, {.Width = Size::Fill()});
                EndVStack();
            });
        // The first quad is the track; its right edge is the stack's (plus one point of quad padding).
        EXPECT_FLOAT_EQ(GetDrawData().Vertices[2].Position.X, 301.0f);
    }

    TEST_F(WidgetTests, CheckboxDrawsItsMark)
    {
        bool value = false;
        bool mixed = false;
        Rect rect;
        const Builder build = [&]
        {
            Toggle("Enable", &value, {.Kind = ToggleKind::Checkbox, .IsMixed = mixed});
            rect = GetItemRect();
        };
        const auto countLines = [&]
        {
            // Lines are squircles whose quads are not axis-aligned or are thin; count the white marks instead.
            int count = 0;
            for (size_t quad = 0; quad < GetDrawData().Vertices.size() / 4; quad++)
            {
                const Color color = GetQuadColor(quad);
                if (color.R > 0.99f && color.G > 0.99f && color.B > 0.99f && color.A > 0.9f &&
                    GetPrimitive(quad * 4).Kind == DrawPrimitiveKind::Squircle &&
                    GetPrimitive(quad * 4).HalfSize.Y < 2.0f)
                    count++;
            }
            return count;
        };

        Settle(build);
        EXPECT_EQ(countLines(), 0);
        EXPECT_FLOAT_EQ(rect.Height, 16.0f); // label line height; the 14-point box is centered in it

        Click(Vec2(rect.X + 7.0f, rect.GetCenter().Y), build);
        EXPECT_TRUE(value);
        Settle(build, 40);
        EXPECT_EQ(countLines(), 2); // the two strokes of a checkmark

        value = false;
        mixed = true;
        Settle(build, 40);
        EXPECT_EQ(countLines(), 1); // a dash
    }

    // ---- Slider ---------------------------------------------------------------------------------------------

    TEST_F(WidgetTests, SliderFollowsClicksAndDrags)
    {
        float value = 0.0f;
        int changes = 0;
        Rect rect;
        const Builder build = [&]
        {
            changes += Slider("Volume", &value, 0.0f, 100.0f, {.Width = 218.0f}) ? 1 : 0;
            rect = GetItemRect();
        };
        Settle(build);
        ASSERT_FLOAT_EQ(rect.Width, 218.0f);

        // The knob (18 points) travels 200 points: from x + 9 to x + 209.
        MoveMouse(Vec2(rect.X + 9.0f + 100.0f, rect.GetCenter().Y), build);
        PressMouse(build);
        EXPECT_FLOAT_EQ(value, 50.0f); // clicking the track moves the knob there
        EXPECT_GE(changes, 1);

        GetIO().AddMousePosEvent(rect.X + 9.0f + 150.0f, rect.GetCenter().Y + 80.0f); // far off the track
        Frame(build);
        EXPECT_FLOAT_EQ(value, 75.0f);

        GetIO().AddMousePosEvent(rect.X + 5000.0f, rect.GetCenter().Y);
        Frame(build);
        EXPECT_FLOAT_EQ(value, 100.0f); // clamped

        ReleaseMouse(build);
        const int settled = changes;
        Frame(build);
        EXPECT_EQ(changes, settled); // no change reported while nothing moves
    }

    TEST_F(WidgetTests, GrabbingTheSliderKnobDoesNotMakeItJump)
    {
        float value = 50.0f;
        Rect rect;
        const Builder build = [&]
        {
            Slider("Level", &value, 0.0f, 100.0f, {.Width = 218.0f});
            rect = GetItemRect();
        };
        Settle(build);
        // Press 6 points right of the knob's center, still on the knob.
        MoveMouse(Vec2(rect.X + 9.0f + 100.0f + 6.0f, rect.GetCenter().Y), build);
        PressMouse(build);
        EXPECT_FLOAT_EQ(value, 50.0f);
        GetIO().AddMousePosEvent(rect.X + 9.0f + 100.0f + 6.0f + 20.0f, rect.GetCenter().Y);
        Frame(build);
        EXPECT_FLOAT_EQ(value, 60.0f);
        ReleaseMouse(build);
    }

    TEST_F(WidgetTests, SliderStepsAndKeyboard)
    {
        float value = 3.0f;
        Rect rect;
        const Builder build = [&]
        {
            Slider("Steps", &value, 0.0f, 10.0f, {.Step = 2.0f, .Width = 218.0f});
            rect = GetItemRect();
        };
        Settle(build);

        // A click at 47 % snaps to the nearest step.
        Click(Vec2(rect.X + 9.0f + 94.0f, rect.GetCenter().Y), build);
        EXPECT_FLOAT_EQ(value, 4.0f);

        // The click focused the slider: arrow keys move by one step, Home and End jump to the ends.
        TapKey(Key::RightArrow, build);
        EXPECT_FLOAT_EQ(value, 6.0f);
        TapKey(Key::UpArrow, build);
        EXPECT_FLOAT_EQ(value, 8.0f);
        TapKey(Key::LeftArrow, build);
        TapKey(Key::DownArrow, build);
        EXPECT_FLOAT_EQ(value, 4.0f);
        TapKey(Key::End, build);
        EXPECT_FLOAT_EQ(value, 10.0f);
        TapKey(Key::RightArrow, build);
        EXPECT_FLOAT_EQ(value, 10.0f); // stays inside the range
        TapKey(Key::Home, build);
        EXPECT_FLOAT_EQ(value, 0.0f);
    }

    TEST_F(WidgetTests, ContinuousSliderMovesByAFractionWithArrowKeys)
    {
        float value = 0.5f;
        const Builder build = [&] { Slider("Opacity", &value, 0.0f, 1.0f); };
        Settle(build);
        TapKey(Key::Tab, build);
        TapKey(Key::RightArrow, build);
        EXPECT_NEAR(value, 0.55f, 1e-5f);
    }

    TEST_F(WidgetTests, InvalidSliderArgumentsAreReported)
    {
        float value = 0.0f;
        Frame([&] { EXPECT_FALSE(Slider("Empty", &value, 5.0f, 5.0f)); });
        EXPECT_EQ(m_AssertMessages.size(), 1u);
        m_AssertMessages.clear();
        Frame([] { EXPECT_FALSE(Slider("Null", nullptr, 0.0f, 1.0f)); });
        EXPECT_EQ(m_AssertMessages.size(), 1u);
        m_AssertMessages.clear();
    }

    // ---- Image ----------------------------------------------------------------------------------------------

    TEST_F(WidgetTests, ImageDrawsTheTexture)
    {
        Rect rect;
        Settle(
            [&]
            {
                Image(TextureID{77}, Vec2(120.0f, 80.0f), {.CornerRadius = 12.0f});
                rect = GetItemRect();
            });
        EXPECT_EQ(rect.GetSize(), Vec2(120.0f, 80.0f));
        const DrawData& drawData = GetDrawData();
        ASSERT_EQ(drawData.Commands.size(), 1u);
        EXPECT_EQ(drawData.Commands[0].Texture, TextureID{77});
        EXPECT_EQ(GetPrimitive(0).Kind, DrawPrimitiveKind::Image);
        EXPECT_FLOAT_EQ(GetPrimitive(0).Radius, 12.0f);
    }

    // ---- Tooltip --------------------------------------------------------------------------------------------

    TEST_F(WidgetTests, TooltipAppearsAfterThePointerRests)
    {
        Rect rect;
        const Builder build = [&]
        {
            Button("Restore");
            rect = GetItemRect();
            Tooltip("Restore default settings");
        };
        const auto hasTooltip = [&] { return !Internal::GetContext().Draw.GetCommands(DrawLayer::Tooltip).empty(); };
        Settle(build);
        EXPECT_FALSE(hasTooltip());

        MoveMouse(rect.GetCenter(), build);
        for (int i = 0; i < 20; i++)
            Frame(build); // a third of a second: not yet
        EXPECT_FALSE(hasTooltip());
        EXPECT_TRUE(IsAnimating()); // the delay keeps frames coming

        for (int i = 0; i < 40; i++)
            Frame(build);
        EXPECT_TRUE(hasTooltip());

        // Moving away hides it again.
        MoveMouse(Vec2(600.0f, 500.0f), build);
        Settle(build, 30);
        EXPECT_FALSE(hasTooltip());
    }

    TEST_F(WidgetTests, TooltipStaysOnTheDisplay)
    {
        GetIO().SetDisplaySize(300.0f, 100.0f);
        const Builder build = [&]
        {
            SetCursorPos(Vec2(230.0f, 70.0f));
            Button("Edge");
            Tooltip("A fairly long description near the corner");
        };
        Settle(build);
        MoveMouse(Vec2(250.0f, 82.0f), build);
        Settle(build, 80);

        const DrawList& drawList = Internal::GetContext().Draw;
        const std::span<const DrawIndex> indices = drawList.GetIndices(DrawLayer::Tooltip);
        ASSERT_FALSE(indices.empty());
        // The tooltip's background (the second quad on the layer, after its shadow) lies inside the display.
        const DrawVertex& topLeft = drawList.GetVertices()[indices[6]];
        const DrawVertex& bottomRight = drawList.GetVertices()[indices[8]];
        EXPECT_GE(topLeft.Position.X, 0.0f);
        EXPECT_GE(topLeft.Position.Y, 0.0f);
        EXPECT_LE(bottomRight.Position.X, 300.0f);
        EXPECT_LE(bottomRight.Position.Y, 100.0f);
    }
} // namespace Carbon
