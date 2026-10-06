// Rows: Table, List, OutlineView and ColumnView with 1,000 to 100,000 rows of which about 30 are visible, scrolled
// to the top and to the middle. Each row is submitted every frame, as an application with a plain loop does.

#include <format>
#include <string>
#include <vector>

#include <Carbon/Extensions/Extensions.h>

#include "Support/FrameBenchmark.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        constexpr float RowHeight = 24.0f;
        // About 30 rows are visible.
        constexpr float ViewHeight = 740.0f;
        constexpr int ChildrenPerGroup = 9;

        // The rows' texts are made once, so that formatting them is not part of a frame.
        struct RowData
        {
            std::vector<std::string> Names;
            std::vector<std::string> Kinds;
            std::vector<std::string> Sizes;
            std::vector<std::string> Groups;
        };

        const RowData& GetRowData(size_t count)
        {
            static RowData s_Data;
            static const char* const s_Kinds[] = {"Document", "Spreadsheet", "Presentation", "Image", "Archive"};
            for (size_t i = s_Data.Names.size(); i < count; i++)
            {
                s_Data.Names.push_back(std::format("Quarterly Report {:06}", i));
                s_Data.Kinds.emplace_back(s_Kinds[i % std::size(s_Kinds)]);
                s_Data.Sizes.push_back(std::format("{} KB", 12 + (i * 37) % 9000));
                s_Data.Groups.push_back(std::format("Folder {:05}", i));
            }
            return s_Data;
        }

        // Builds the interface until its content has been measured, then scrolls it with `scroll`, which runs at
        // the start of two frames.
        template <typename Build, typename Scroll>
        void Prepare(FrameBenchmark& frame, const Build& build, const Scroll& scroll)
        {
            for (int i = 0; i < 3; i++)
                frame.RunFrame(build, 0.25f);
            for (int i = 0; i < 2; i++)
            {
                frame.RunFrame(
                    [&]
                    {
                        scroll();
                        build();
                    },
                    0.25f);
            }
        }

        // The offset that puts the middle row at the top of the view, and a row a few below it to select.
        float GetRowOffset(const benchmark::State& state)
        {
            return state.range(1) != 0 ? RowHeight * static_cast<float>(state.range(0) / 2) : 0.0f;
        }

        int GetSelectedRow(const benchmark::State& state)
        {
            return static_cast<int>(GetRowOffset(state) / RowHeight) + 3;
        }

        void Table(benchmark::State& state)
        {
            const int rowCount = static_cast<int>(state.range(0));
            const RowData& data = GetRowData(static_cast<size_t>(rowCount));
            const float offset = GetRowOffset(state);
            const int selected = GetSelectedRow(state);
            static const TableColumn s_Columns[] = {
                {.Title = "Name"},
                {.Title = "Kind", .Width = 180.0f},
                {.Title = "Size", .Width = 110.0f, .Alignment = TextAlignment::Trailing},
            };

            FrameBenchmark frame;
            const auto build = [&]
            {
                BeginVStack({.Padding = 20.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                BeginTable("table", s_Columns, {.Height = ViewHeight});
                for (int i = 0; i < rowCount; i++)
                {
                    const size_t index = static_cast<size_t>(i);
                    TableRow(i, i == selected);
                    TableCell(data.Names[index]);
                    TableCell(data.Kinds[index], {.Secondary = true});
                    TableCell(data.Sizes[index], {.Secondary = true});
                }
                EndTable();
                EndVStack();
            };
            Prepare(frame, build,
                    [&]
                    {
                        PushID("table");
                        SetScrollOffset("##rows", Vec2(0.0f, offset));
                        PopID();
                    });
            frame.Measure(state, build);
        }

        void List(benchmark::State& state)
        {
            const int rowCount = static_cast<int>(state.range(0));
            const RowData& data = GetRowData(static_cast<size_t>(rowCount));
            const float offset = GetRowOffset(state);
            const int selected = GetSelectedRow(state);

            FrameBenchmark frame;
            const auto build = [&]
            {
                BeginVStack({.Padding = 20.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                BeginList("list", {.Height = ViewHeight});
                for (int i = 0; i < rowCount; i++)
                {
                    const size_t index = static_cast<size_t>(i);
                    ListItem(data.Names[index], i == selected, {.Icon = Icons::File, .Detail = data.Sizes[index]});
                }
                EndList();
                EndVStack();
            };
            Prepare(frame, build, [&] { SetScrollOffset("list", Vec2(0.0f, offset)); });
            frame.Measure(state, build);
        }

        // Folders of nine files each, all expanded: `rows` counts folders and files.
        void OutlineView(benchmark::State& state)
        {
            const int rowCount = static_cast<int>(state.range(0));
            const int groupCount = rowCount / (ChildrenPerGroup + 1);
            const RowData& data = GetRowData(static_cast<size_t>(rowCount));
            const float offset = GetRowOffset(state);
            const int selected = GetSelectedRow(state);

            FrameBenchmark frame;
            const auto build = [&]
            {
                BeginVStack({.Padding = 20.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                BeginOutlineView("outline", {.Height = ViewHeight});
                int row = 0;
                for (int group = 0; group < groupCount; group++)
                {
                    const OutlineItem folder =
                        BeginOutlineItem(data.Groups[static_cast<size_t>(group)], row == selected,
                                         {.Icon = Icons::Folder, .IsInitiallyExpanded = true});
                    row++;
                    if (folder.IsExpanded)
                    {
                        for (int child = 0; child < ChildrenPerGroup; child++)
                        {
                            BeginOutlineItem(data.Names[static_cast<size_t>(row)], row == selected,
                                             {.Icon = Icons::File, .HasChildren = false});
                            EndOutlineItem();
                            row++;
                        }
                    }
                    EndOutlineItem();
                }
                EndOutlineView();
                EndVStack();
            };
            Prepare(frame, build, [&] { SetScrollOffset("outline", Vec2(0.0f, offset)); });
            frame.Measure(state, build);
        }

        // One long column of folders with one selected, and its 30 children in the next column.
        void ColumnView(benchmark::State& state)
        {
            const int rowCount = static_cast<int>(state.range(0));
            const RowData& data = GetRowData(static_cast<size_t>(rowCount));
            const float offset = GetRowOffset(state);
            const int selected = GetSelectedRow(state);

            FrameBenchmark frame;
            const auto build = [&]
            {
                BeginVStack({.Padding = 20.0f, .Width = Size::Fill(), .Height = Size::Fill()});
                BeginColumnView("columns", {.Height = ViewHeight, .ColumnWidth = 320.0f});
                BeginColumnViewColumn();
                for (int i = 0; i < rowCount; i++)
                {
                    ColumnViewItem(data.Groups[static_cast<size_t>(i)], i == selected,
                                   {.Icon = Icons::Folder, .HasChildren = true});
                }
                EndColumnViewColumn();
                BeginColumnViewColumn();
                for (int i = 0; i < 30; i++)
                    ColumnViewItem(data.Names[static_cast<size_t>(i)], false, {.Icon = Icons::File});
                EndColumnViewColumn();
                EndColumnView();
                EndVStack();
            };
            Prepare(frame, build,
                    [&]
                    {
                        const int64_t firstColumn = 0;
                        PushID("columns");
                        PushID(firstColumn);
                        SetScrollOffset("##column", Vec2(0.0f, offset));
                        PopID();
                        PopID();
                    });
            frame.Measure(state, build);
        }

        void ApplyRowArguments(benchmark::Benchmark* benchmark)
        {
            benchmark->ArgNames({"rows", "middle"});
            for (const int64_t rows : {1000, 5000, 50000, 100000})
            {
                for (const int64_t middle : {0, 1})
                    benchmark->Args({rows, middle});
            }
        }
    } // namespace

    CB_FRAME_BENCHMARK(Table)->Name("Rows/Table")->Apply(ApplyRowArguments);
    CB_FRAME_BENCHMARK(List)->Name("Rows/List")->Apply(ApplyRowArguments);
    CB_FRAME_BENCHMARK(OutlineView)->Name("Rows/OutlineView")->Apply(ApplyRowArguments);
    CB_FRAME_BENCHMARK(ColumnView)->Name("Rows/ColumnView")->Apply(ApplyRowArguments);
} // namespace Carbon::Benchmarks
