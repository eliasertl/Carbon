// Glyph atlas: many distinct glyphs and sizes. One scenario keeps a large, fixed set of glyphs in use; the other
// asks for new sizes in every frame until the atlas overflows, as text that is zoomed does.

#include <string>

#include "Support/FrameBenchmark.h"

namespace Carbon::Benchmarks
{
    namespace
    {
        // The printable ASCII characters and a row of Latin-1 letters.
        const std::string& GetAlphabet()
        {
            static const std::string s_Alphabet = []
            {
                std::string text;
                for (char c = '!'; c <= '~'; c++)
                    text.push_back(c);
                text +=
                    "\xC3\x84\xC3\x96\xC3\x9C\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x9F\xC3\xA9\xC3\xA8\xC3\xA0\xC3\xA7\xC3\xB1";
                return text;
            }();
            return s_Alphabet;
        }

        // 24 sizes in three weights, every frame the same: about 7,000 glyph quads from a cache of thousands of
        // glyphs.
        void ManyGlyphs(benchmark::State& state)
        {
            FrameBenchmark frame;
            const auto build = [&]
            {
                DrawList& drawList = GetDrawList();
                const Color color = GetStyleColor(StyleColor::Label);
                float y = 4.0f;
                for (int i = 0; i < 24; i++)
                {
                    TextSpec spec = GetTextSpec(TextStyle::Body);
                    spec.Size = 9.0f + static_cast<float>(i);
                    for (const FontWeight weight : {FontWeight::Regular, FontWeight::Medium, FontWeight::Bold})
                    {
                        spec.Weight = weight;
                        drawList.AddText(Vec2(4.0f, y), GetAlphabet(), spec, color);
                        y += 10.0f;
                    }
                }
            };
            frame.Measure(state, build);
        }

        // A line of text whose size changes in every frame, cycling through 1,200 sizes: every frame rasterizes
        // about a hundred new glyphs, and the atlas overflows several times per cycle.
        void ChangingSizes(benchmark::State& state)
        {
            // Carbon warns each time the atlas is full, which is what this scenario is about.
            FrameBenchmark frame({.PrintsWarnings = false});
            uint64_t tick = 0;
            const auto build = [&]
            {
                tick++;
                TextSpec spec = GetTextSpec(TextStyle::Body);
                spec.Size = 8.0f + static_cast<float>(tick % 1200) * 0.05f;
                GetDrawList().AddText(Vec2(4.0f, 100.0f), GetAlphabet(), spec, GetStyleColor(StyleColor::Label));
            };
            frame.Measure(state, build);
        }
    } // namespace

    CB_FRAME_BENCHMARK(ManyGlyphs)->Name("GlyphAtlas/ManyGlyphs");
    CB_FRAME_BENCHMARK(ChangingSizes)->Name("GlyphAtlas/ChangingSizes")->MinTime(2.0);
} // namespace Carbon::Benchmarks
