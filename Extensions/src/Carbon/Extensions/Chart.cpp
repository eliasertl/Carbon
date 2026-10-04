#include "Carbon/Extensions/Chart.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <string>

namespace Carbon
{
    namespace
    {
        constexpr float LegendHeight = 22.0f;
        constexpr float AxisGap = 6.0f;
        constexpr float LineWidth = 2.0f;
        constexpr float PointRadius = 3.0f;
        constexpr float BarCornerRadius = 3.0f;
        // Share of a category's slot that its bars take up.
        constexpr float BarGroupWidth = 0.72f;
        constexpr float BarGap = 2.0f;
        constexpr AnimationSpec AppearSpring = AnimationSpec::Spring(0.6f);

        constexpr StyleColor Palette[] = {StyleColor::Accent, StyleColor::Green, StyleColor::Orange,
                                          StyleColor::Purple, StyleColor::Red,   StyleColor::Teal,
                                          StyleColor::Pink,   StyleColor::Yellow};

        // Scratch text for numbers and the callout; reused so that drawing a chart does not allocate.
        std::string s_Text;

        struct ChartState
        {
            bool IsShown;
        };

        // What both kinds of chart work out before drawing their marks.
        struct ChartFrame
        {
            ID Id;
            Rect Bounds;
            Rect Plot;
            float Min = 0.0f;
            float Max = 1.0f;
            int Count = 0;
            /// Bars sit in the middle of their slot; line points go from edge to edge.
            bool CentersValues = false;
            /// 0..1 while the chart draws itself in.
            float Appear = 1.0f;
            /// The value under the pointer, or -1.
            int Hovered = -1;

            float GetX(int index) const
            {
                if (CentersValues)
                    return Plot.X + Plot.Width * (static_cast<float>(index) + 0.5f) / static_cast<float>(Count);
                return Count > 1 ? Plot.X + Plot.Width * static_cast<float>(index) / static_cast<float>(Count - 1)
                                 : Plot.GetCenter().X;
            }

            float GetY(float value) const { return Plot.GetBottom() - (value - Min) / (Max - Min) * Plot.Height; }
        };

        Color GetSeriesColor(const ChartSeries& series, size_t index)
        {
            return series.Color.value_or(GetStyleColor(Palette[index % std::size(Palette)]));
        }

        std::string_view FormatNumber(float value)
        {
            s_Text.clear();
            std::format_to(std::back_inserter(s_Text), "{:.6g}", value);
            return s_Text;
        }

        // A step of 1, 2 or 5 times a power of ten that gives about four intervals.
        float GetNiceStep(float range)
        {
            const float raw = range / 4.0f;
            const float magnitude = std::pow(10.0f, std::floor(std::log10(raw)));
            const float normalized = raw / magnitude;
            const float nice =
                normalized < 1.5f ? 1.0f : (normalized < 3.0f ? 2.0f : (normalized < 7.0f ? 5.0f : 10.0f));
            return nice * magnitude;
        }

        // Reserves the chart's space and draws everything but the marks: legend, grid and axis labels.
        ChartFrame BeginChart(std::string_view id, std::span<const ChartSeries> series, const ChartOptions& options,
                              bool isBarChart)
        {
            ChartFrame frame;
            frame.Id = GetID(id);
            frame.CentersValues = isBarChart;

            ItemOptions item;
            item.Width = options.Width;
            frame.Bounds = AllocateItem(Vec2(260.0f, options.Height), item);

            DrawList& drawList = GetDrawList();
            const ContentScale scale = GetContentScale();
            const float pixel = scale.GetPixelSize();
            const Color secondary = GetStyleColor(StyleColor::SecondaryLabel);

            // The range of the data.
            float low = 0.0f;
            float high = 0.0f;
            bool hasValue = false;
            bool hasLegend = false;
            for (const ChartSeries& entry : series)
            {
                frame.Count = std::max(frame.Count, static_cast<int>(entry.Values.size()));
                hasLegend = hasLegend || !entry.Label.empty();
                for (const float value : entry.Values)
                {
                    low = hasValue ? std::min(low, value) : value;
                    high = hasValue ? std::max(high, value) : value;
                    hasValue = true;
                }
            }
            if (isBarChart)
            {
                low = std::min(low, 0.0f);
                high = std::max(high, 0.0f);
            }
            low = options.Min.value_or(low);
            high = options.Max.value_or(high);
            if (high <= low)
                high = low + 1.0f;
            const float step = GetNiceStep(high - low);
            if (!options.Min.has_value())
                low = std::floor(low / step) * step;
            if (!options.Max.has_value())
                high = std::ceil(high / step) * step;
            frame.Min = low;
            frame.Max = high;

            // Legend: a dot and a label per series, above the plot.
            float top = frame.Bounds.Y;
            if (options.ShowsLegend && hasLegend)
            {
                const TextSpec spec = GetTextSpec(TextStyle::Subheadline);
                const Rect row(frame.Bounds.X, top, frame.Bounds.Width, LegendHeight - 6.0f);
                float x = frame.Bounds.X + 2.0f;
                for (size_t i = 0; i < series.size(); i++)
                {
                    if (series[i].Label.empty())
                        continue;
                    drawList.AddCircle(Vec2(x + 4.0f, row.GetCenter().Y), 4.0f, GetSeriesColor(series[i], i));
                    DrawLabel(drawList, row, x + 13.0f, series[i].Label, spec, secondary);
                    x += 13.0f + MeasureText(series[i].Label, spec).X + 14.0f;
                }
                top += LegendHeight;
            }

            // The plot leaves room for the value labels at the leading edge and the category labels below.
            const TextSpec tickSpec = GetTextSpec(TextStyle::Caption1);
            const float tickHeight = GetFontMetrics(tickSpec).LineHeight;
            const int tickCount = static_cast<int>(std::round((high - low) / step));
            float gutter = 0.0f;
            for (int i = 0; i <= tickCount; i++)
                gutter = std::max(gutter, MeasureText(FormatNumber(low + step * static_cast<float>(i)), tickSpec).X);
            gutter += AxisGap;
            const float footer = options.Labels.empty() ? tickHeight * 0.5f : tickHeight + AxisGap;
            frame.Plot = Rect(frame.Bounds.X + gutter, top + tickHeight * 0.5f, frame.Bounds.Width - gutter - 4.0f,
                              frame.Bounds.GetBottom() - footer - top - tickHeight * 0.5f);
            if (frame.Plot.Width <= 1.0f || frame.Plot.Height <= 1.0f)
            {
                frame.Count = 0;
                return frame;
            }

            for (int i = 0; i <= tickCount; i++)
            {
                const float value = low + step * static_cast<float>(i);
                const float y = scale.Snap(frame.GetY(value));
                if (options.ShowsGrid || i == 0)
                {
                    // The line at zero, or else the lowest one, is the axis and is drawn stronger.
                    const bool isAxis = std::abs(value) < step * 0.01f || (i == 0 && low > 0.0f);
                    drawList.AddRect(Rect(frame.Plot.X, y, frame.Plot.Width, pixel),
                                     GetStyleColor(isAxis ? StyleColor::ControlBorder : StyleColor::Separator));
                }
                const std::string_view label = FormatNumber(value);
                const float width = MeasureText(label, tickSpec).X;
                drawList.AddText(Vec2(frame.Plot.X - AxisGap - width, y - tickHeight * 0.5f), label, tickSpec,
                                 secondary);
            }

            if (!options.Labels.empty() && frame.Count > 0)
            {
                float widest = 0.0f;
                for (const std::string_view label : options.Labels)
                    widest = std::max(widest, MeasureText(label, tickSpec).X);
                const float slot =
                    frame.Plot.Width / static_cast<float>(isBarChart ? frame.Count : std::max(frame.Count - 1, 1));
                const int stride = std::max(static_cast<int>(std::ceil((widest + 8.0f) / slot)), 1);
                const int labelCount = std::min(frame.Count, static_cast<int>(options.Labels.size()));
                for (int i = 0; i < labelCount; i += stride)
                {
                    const std::string_view label = options.Labels[static_cast<size_t>(i)];
                    const float width = MeasureText(label, tickSpec).X;
                    const float x = std::clamp(frame.GetX(i) - width * 0.5f, frame.Bounds.X,
                                               std::max(frame.Bounds.X, frame.Bounds.GetRight() - width));
                    drawList.AddText(Vec2(x, frame.Plot.GetBottom() + AxisGap), label, tickSpec, secondary);
                }
            }

            // The chart draws itself in when it appears.
            const ID appear = HashID("##appear", frame.Id);
            ChartState& state = *GetState<ChartState>(frame.Id);
            if (!state.IsShown)
            {
                SetAnimationValue(appear, 0.0f);
                state.IsShown = true;
            }
            frame.Appear = std::clamp(Animate(appear, 1.0f, AppearSpring), 0.0f, 1.0f);

            if (frame.Count > 0 && IsRectHovered(frame.Plot))
            {
                const float position = (GetMousePos().X - frame.Plot.X) / frame.Plot.Width;
                const float index = isBarChart ? std::floor(position * static_cast<float>(frame.Count))
                                               : std::round(position * static_cast<float>(frame.Count - 1));
                frame.Hovered = std::clamp(static_cast<int>(index), 0, frame.Count - 1);
            }
            return frame;
        }

        // Names the values under the pointer in a small box next to it, above everything else.
        void DrawCallout(const ChartFrame& frame, std::span<const ChartSeries> series, const ChartOptions& options)
        {
            const size_t index = static_cast<size_t>(frame.Hovered);
            s_Text.clear();
            if (index < options.Labels.size())
                s_Text.append(options.Labels[index]);
            for (const ChartSeries& entry : series)
            {
                if (index >= entry.Values.size())
                    continue;
                if (!s_Text.empty())
                    s_Text.push_back('\n');
                if (!entry.Label.empty())
                    s_Text.append(entry.Label).append(": ");
                std::format_to(std::back_inserter(s_Text), "{:.6g}", entry.Values[index]);
            }
            if (s_Text.empty())
                return;

            const TextSpec spec = GetTextSpec(TextStyle::Subheadline);
            const Vec2 padding(8.0f, 5.0f);
            const Vec2 size = MeasureText(s_Text, spec) + padding * 2.0f;
            const Vec2 display = GetDisplaySize();
            const Vec2 mouse = GetMousePos();
            // Beside the pointer; on its other side when the display ends.
            Vec2 origin(mouse.X + 14.0f, mouse.Y - size.Y - 6.0f);
            if (origin.X + size.X > display.X - 4.0f)
                origin.X = mouse.X - 14.0f - size.X;
            origin.Y = std::clamp(origin.Y, 4.0f, std::max(4.0f, display.Y - size.Y - 4.0f));
            const Rect rect = GetContentScale().Snap(Rect(origin, size));

            DrawList& drawList = GetDrawList();
            const float radius = 5.0f;
            drawList.PushLayer(DrawLayer::Tooltip);
            drawList.AddShadow(rect, GetStyleColor(StyleColor::Shadow), radius, 8.0f, Vec2(0.0f, 2.0f));
            drawList.AddSquircle(rect, GetStyleColor(StyleColor::OverlayBackground), radius);
            drawList.AddSquircleStroke(rect, GetStyleColor(StyleColor::OverlayBorder), radius,
                                       GetContentScale().GetPixelSize());
            drawList.AddText(rect.GetMin() + padding, s_Text, spec, GetStyleColor(StyleColor::Label));
            drawList.PopLayer();
        }

        void EndChart(const ChartFrame& frame)
        {
            Interaction interaction;
            interaction.Hovered = IsRectHovered(frame.Bounds);
            SetLastItem(frame.Id, frame.Bounds, interaction);
        }
    } // namespace

    void LineChart(std::string_view id, std::span<const ChartSeries> series, const ChartOptions& options)
    {
        const ChartFrame frame = BeginChart(id, series, options, false);
        if (frame.Count == 0)
        {
            EndChart(frame);
            return;
        }
        DrawList& drawList = GetDrawList();

        // The lines are revealed from the leading edge.
        const float margin = PointRadius + 2.0f;
        drawList.PushClipRect(Rect(frame.Plot.X - margin, frame.Plot.Y - margin,
                                   (frame.Plot.Width + margin * 2.0f) * frame.Appear,
                                   frame.Plot.Height + margin * 2.0f));
        for (size_t s = 0; s < series.size(); s++)
        {
            const std::span<const float> values = series[s].Values;
            const Color color = GetSeriesColor(series[s], s);
            Vec2 previous;
            for (size_t i = 0; i < values.size(); i++)
            {
                const Vec2 point(frame.GetX(static_cast<int>(i)), frame.GetY(values[i]));
                if (i > 0)
                    drawList.AddLine(previous, point, color, LineWidth);
                previous = point;
            }
            if (options.ShowsPoints || values.size() == 1)
            {
                for (size_t i = 0; i < values.size(); i++)
                    drawList.AddCircle(Vec2(frame.GetX(static_cast<int>(i)), frame.GetY(values[i])), PointRadius,
                                       color);
            }
        }
        drawList.PopClipRect();

        if (frame.Hovered >= 0)
        {
            // A guide through the values under the pointer, each marked with a ringed dot.
            const float x = frame.GetX(frame.Hovered);
            const float pixel = GetContentScale().GetPixelSize();
            drawList.AddRect(Rect(GetContentScale().Snap(x), frame.Plot.Y, pixel, frame.Plot.Height),
                             GetStyleColor(StyleColor::ControlBorder));
            for (size_t s = 0; s < series.size(); s++)
            {
                if (static_cast<size_t>(frame.Hovered) >= series[s].Values.size())
                    continue;
                const Vec2 point(x, frame.GetY(series[s].Values[static_cast<size_t>(frame.Hovered)]));
                drawList.AddCircle(point, PointRadius + 2.0f, GetStyleColor(StyleColor::Background));
                drawList.AddCircle(point, PointRadius + 0.5f, GetSeriesColor(series[s], s));
            }
            DrawCallout(frame, series, options);
        }
        EndChart(frame);
    }

    void LineChart(std::string_view id, std::span<const float> values, const ChartOptions& options)
    {
        ChartSeries series;
        series.Values = values;
        LineChart(id, std::span<const ChartSeries>(&series, 1), options);
    }

    void BarChart(std::string_view id, std::span<const ChartSeries> series, const ChartOptions& options)
    {
        const ChartFrame frame = BeginChart(id, series, options, true);
        if (frame.Count == 0 || series.empty())
        {
            EndChart(frame);
            return;
        }
        DrawList& drawList = GetDrawList();
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);

        const float slot = frame.Plot.Width / static_cast<float>(frame.Count);
        const float seriesCount = static_cast<float>(series.size());
        const float group = slot * BarGroupWidth;
        const float gap = series.size() > 1 ? std::min(BarGap, group * 0.1f) : 0.0f;
        const float barWidth = std::max((group - gap * (seriesCount - 1.0f)) / seriesCount, 1.0f);
        const float radius = std::min(BarCornerRadius, barWidth * 0.5f);
        const float zero = frame.GetY(std::clamp(0.0f, frame.Min, frame.Max));

        if (frame.Hovered >= 0)
        {
            drawList.AddSquircle(
                Rect(frame.Plot.X + slot * static_cast<float>(frame.Hovered), frame.Plot.Y, slot, frame.Plot.Height),
                GetStyleColor(StyleColor::ControlFill).WithOpacity(0.5f), 4.0f, smoothing);
        }

        // Bars are rounded only at their far end: they are drawn longer than they are and cut off at the zero
        // line. Bars above the line and bars below it need different cuts, hence two passes.
        for (int pass = 0; pass < 2; pass++)
        {
            const bool isUpward = pass == 0;
            drawList.PushClipRect(
                isUpward ? Rect(frame.Plot.X, frame.Plot.Y - 1.0f, frame.Plot.Width, zero - frame.Plot.Y + 1.0f)
                         : Rect(frame.Plot.X, zero, frame.Plot.Width, frame.Plot.GetBottom() - zero + 1.0f));
            for (size_t s = 0; s < series.size(); s++)
            {
                const std::span<const float> values = series[s].Values;
                const Color color = GetSeriesColor(series[s], s);
                for (size_t i = 0; i < values.size(); i++)
                {
                    if ((values[i] >= 0.0f) != isUpward)
                        continue;
                    const float x =
                        frame.GetX(static_cast<int>(i)) - group * 0.5f + (barWidth + gap) * static_cast<float>(s);
                    // Bars grow out of the zero line when the chart appears.
                    const float length = std::abs(frame.GetY(values[i]) - zero) * frame.Appear;
                    if (length <= 0.0f)
                        continue;
                    const Rect bar = isUpward ? Rect(x, zero - length, barWidth, length + radius)
                                              : Rect(x, zero - radius, barWidth, length + radius);
                    drawList.AddSquircle(bar, color, radius, smoothing);
                }
            }
            drawList.PopClipRect();
        }

        if (frame.Hovered >= 0)
            DrawCallout(frame, series, options);
        EndChart(frame);
    }

    void BarChart(std::string_view id, std::span<const float> values, const ChartOptions& options)
    {
        ChartSeries series;
        series.Values = values;
        BarChart(id, std::span<const ChartSeries>(&series, 1), options);
    }
} // namespace Carbon
