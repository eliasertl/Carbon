// Controls that choose from a list of items: PopUpButton, SegmentedControl and ComboBox, closed and open.

#include <format>
#include <string>
#include <vector>

#include <Carbon/Extensions/Extensions.h>

#include "Support/FrameBenchmark.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        // Item texts and views of them, made once per benchmark.
        struct Items
        {
            explicit Items(size_t count)
            {
                Texts.reserve(count);
                for (size_t i = 0; i < count; i++)
                    Texts.push_back(std::format("Option {:05}", i));
                Views.assign(Texts.begin(), Texts.end());
            }

            std::vector<std::string> Texts;
            std::vector<std::string_view> Views;
        };

        // A closed pop-up button: only the button is on screen.
        void PopUpButtonClosed(benchmark::State& state)
        {
            const Items items(static_cast<size_t>(state.range(0)));
            int selected = 0;
            FrameBenchmark frame;
            frame.Measure(state,
                          [&]
                          {
                              BeginVStack({.Padding = 20.0f});
                              PopUpButton("Choice", &selected, items.Views);
                              EndVStack();
                          });
        }

        void SegmentedControlSegments(benchmark::State& state)
        {
            const Items items(static_cast<size_t>(state.range(0)));
            int selected = 0;
            FrameBenchmark frame;
            frame.Measure(state,
                          [&]
                          {
                              BeginVStack({.Padding = 20.0f});
                              SegmentedControl("View", &selected, items.Views);
                              EndVStack();
                          });
        }

        // A combo box with its list open: all items after the down arrow, or the matching ones after typing "9".
        void ComboBoxOpen(benchmark::State& state)
        {
            const Items items(static_cast<size_t>(state.range(0)));
            const bool isFiltered = state.range(1) != 0;
            std::string text;
            FrameBenchmark frame;
            const auto build = [&]
            {
                BeginVStack({.Padding = 20.0f});
                ComboBox("Option", &text, items.Views);
                EndVStack();
            };
            frame.RunFrame(build);
            frame.RunFrame(
                [&]
                {
                    SetFocus(GetID("Option"));
                    build();
                });
            frame.RunFrame(build);
            if (isFiltered)
            {
                GetIO().AddInputCharactersUTF8("9");
            }
            else
            {
                GetIO().AddKeyEvent(Key::DownArrow, true);
                frame.RunFrame(build);
                GetIO().AddKeyEvent(Key::DownArrow, false);
            }
            frame.Measure(state, build);
        }
    } // namespace

    CB_FRAME_BENCHMARK(PopUpButtonClosed)->Name("Controls/PopUpButton")->ArgName("items")->Arg(10)->Arg(100)->Arg(1000);
    CB_FRAME_BENCHMARK(SegmentedControlSegments)
        ->Name("Controls/SegmentedControl")
        ->ArgName("segments")
        ->Arg(4)
        ->Arg(12);
    CB_FRAME_BENCHMARK(ComboBoxOpen)
        ->Name("Controls/ComboBoxOpen")
        ->ArgNames({"items", "filtered"})
        ->ArgsProduct({{100, 1000, 10000}, {0, 1}});
} // namespace Carbon::Benchmarks
