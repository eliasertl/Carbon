#pragma once

#include <optional>
#include <source_location>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Core/EdgeInsets.h"
#include "Carbon/Layout/Size.h"

namespace Carbon
{
    /// Options of a vertical stack. All fields are optional.
    struct VStackOptions
    {
        /// Distance between items; the theme's Spacing when not set.
        std::optional<float> Spacing = {};
        /// Space between the stack's edges and its items.
        EdgeInsets Padding = {};
        /// Where items sit horizontally inside the stack.
        Carbon::Alignment Alignment = Carbon::Alignment::Leading;
        /// Where the items sit vertically when the stack is taller than its content and has no Spacer.
        VerticalAlignment Justify = VerticalAlignment::Top;
        Size Width = Size::Fit();
        Size Height = Size::Fit();
        /// Draws a squircle behind the stack, e.g. for a grouped box.
        std::optional<Color> Background = {};
        /// Corner radius of the background; the theme's GroupCornerRadius when not set.
        std::optional<float> CornerRadius = {};
        /// A stable identity. Only needed when the same call site creates several stacks whose order changes;
        /// by default a stack is identified by where in the source it is called from.
        std::string_view ID = {};
    };

    /// Options of a horizontal stack. All fields are optional.
    struct HStackOptions
    {
        std::optional<float> Spacing = {};
        EdgeInsets Padding = {};
        /// Where items sit vertically inside the stack.
        VerticalAlignment Alignment = VerticalAlignment::Center;
        /// Where the items sit horizontally when the stack is wider than its content and has no Spacer.
        Carbon::Alignment Justify = Carbon::Alignment::Leading;
        Size Width = Size::Fit();
        Size Height = Size::Fit();
        std::optional<Color> Background = {};
        std::optional<float> CornerRadius = {};
        std::string_view ID = {};
    };

    /// Options of a spacer.
    struct SpacerOptions
    {
        /// A fixed length in points. When set, the spacer does not stretch.
        std::optional<float> Length = {};
        /// The least space a flexible spacer takes.
        float MinLength = 0.0f;
        /// Share of the free space relative to other spacers and Fill items.
        float Weight = 1.0f;
    };

    /// Starts a stack that lays its items out top to bottom. Every Begin needs a matching End.
    /// Leave `location` alone: it identifies the stack by its call site.
    void BeginVStack(const VStackOptions& options = {},
                     const std::source_location& location = std::source_location::current());
    void EndVStack();

    /// Starts a stack that lays its items out leading to trailing.
    void BeginHStack(const HStackOptions& options = {},
                     const std::source_location& location = std::source_location::current());
    void EndHStack();

    /// Takes up the free space of the current stack along its axis, pushing the following items to the far end.
    /// Several spacers share the space. With `Length` set it is a fixed gap instead.
    void Spacer(const SpacerOptions& options = {});
} // namespace Carbon
