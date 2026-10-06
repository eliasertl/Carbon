# Optimizations

What Carbon costs at run time and at build time, measured before and after each round of optimization. Three
versions are compared:

- **v1**: the baseline, commit `3f9a41d`, before any optimization.
- **v2**: after the runtime optimizations, commit `0e4f99f`.
- **v3**: after the build and CI optimizations, commit `66272b9`.

Every optimization in the lists at the end was kept because its numbers say so; the ones that were tried and did
not pay are listed with their numbers too.

## What is measured, and how

`CarbonBenchmarks` (in `Benchmarks/`, built on [Google Benchmark](https://github.com/google/benchmark)) runs the
runtime measurements. Except for the backend benchmarks it needs no GPU: each scenario creates a headless context
with a display of 1280 x 800 points at content scale 1 (the Gallery frames use the example's 1040 x 740), builds
its interface for ten frames of a quarter second and four of 1/60 s, so that layout, appear fades and scroll
indicators have settled, and then measures steady-state frames. Per frame, from `NewFrame` to `EndFrame`:

- **Frame time**: the median over the frames of a run (`p50_us`) and the 95th percentile (`p95_us`). The tables
  show the median of three runs of each.
- **Heap allocations**: calls of the global `operator new`, which the benchmark executable replaces to count them.
  This includes allocations of the interface code itself, such as a `std::string` an example builds per frame.
- **Draw data**: vertices, indices, primitives, draw commands and their bytes, and the glyph-atlas bytes handed to
  the renderer backend (a recording backend counts them).
- **Backend render time**: the CPU time of `<Backend>Render` alone, for every backend compiled in, through the
  offscreen harnesses of the renderer tests (`Tests/src/Support/*Harness.cpp`). Creating the target and reading it
  back are not part of the time. A backend whose device cannot be created is skipped with a message. Two scenes
  (the renderer tests' scene with 85 quads, and a window full of text with 10,621 quads), each rendered after a new
  frame and rendered again without one, as a host does that redraws a window.

Run them from a Release build:

```sh
cmake --build Build/Release --target CarbonBenchmarks
python Scripts/Benchmarks.py --out Scratch/Benchmarks.json          # everything, median of 3 repetitions
python Scripts/Benchmarks.py --filter "Rows/" --out Scratch/Rows.json
python Scripts/Benchmarks.py --table Scratch/Before.json Scratch/After.json     # Markdown rows, side by side
Build/Release/Benchmarks/CarbonBenchmarks --benchmark_filter="Text/" --benchmark_counters_tabular=true
```

`CARBON_BUILD_BENCHMARKS` (on when Carbon is the top-level project) builds the executable; Google Benchmark is the
submodule `ThirdParty/GoogleBenchmark`, behind `CARBON_DEPS_GOOGLEBENCHMARK_BUILD` and `_NAME` like the other
dependencies. CI builds the benchmarks but does not run them: the timings of shared runners are too noisy.

Build and test costs come from `Scripts/BuildMetrics.py`, run from a Visual Studio developer prompt on an otherwise
idle machine:

```sh
python Scripts/BuildMetrics.py --dawn-debug <dawn>/install/Debug --dawn-release <dawn>/install/Release --out Scratch/BuildMetrics.json
```

It configures an empty build directory with Ninja and builds all targets, three times per configuration; rebuilds
after touching `Framework/src/Carbon/Core/Log.h` and after changing the project version; runs `ctest`; and reads
the job durations of the latest green CI run on `main` from GitHub's public API. "CPU seconds" are the durations of
all build steps in `.ninja_log` added up: what the build would take on one core.

### Machine

All numbers come from one machine, so only their ratios carry over to others:

| | |
| --- | --- |
| CPU | AMD Ryzen AI 7 350 (8 cores, 16 threads), 32 GB RAM |
| GPU | AMD Radeon 860M (integrated), driver 32.0.22024.3004 |
| OS | Windows 11 Home, build 26200 |
| Compiler | MSVC 19.50.35717 (Visual Studio 2026), Ninja, CMake 4.1 |
| Runtime numbers | Release (`/O2`), median of 3 runs |
| Build numbers | Debug and Release, all six renderer backends, warnings as errors, median of 3 runs |
| Dawn | commit `b52750f1cc16802c0c8af0bdbaf5bebcb0d14168`, installed, not built by Carbon |

CI durations are those of GitHub's hosted runners (`windows-2022`, `ubuntu-24.04`) and vary from run to run by some
ten percent.

### The keep rule

An optimization stays only if

1. its metric improves by at least 10 % (or allocations per frame drop to 0), 2. all tests pass in Debug and
Release with zero warnings in Carbon's targets, and 3. `python Scripts/Screenshots.py --build Build/Release
--check` reports the documentation images unchanged.

Otherwise it is reverted and listed under [Tried and rejected](#tried-and-rejected) with what was measured.

In the tables, the change is relative to v1. Runtime rows leave v3 empty unless they were measured again; build
rows show `---` under v2, which does not touch them.

## Rows

Table, List, OutlineView and ColumnView in a view that shows about 30 rows of 24 points, with 1,000 to 100,000 rows
and one row selected. The application submits every row in a plain loop, as the components' documentation shows
(`for (i...) TableRow(i)`); the cases marked "visible rows only" submit the rows that `ClipTableRows`,
`ClipListItems` and `ClipColumnViewItems` return, which exist from v2 on. The table has three text columns, the
list items have an icon and a detail text, the outline view is folders of nine files each, all expanded, and the
column view has one long column and a second one of 30 items. "At the top" is scrolled to the first row, everything
else to the middle of the rows.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | Table, 1,000 rows | 565 µs | 97.9 µs |  | −83% (v1→v2) |
| Frame time (median) | Table, 5,000 rows | 2.75 ms | 369 µs |  | −87% (v1→v2) |
| Frame time (median) | Table, 50,000 rows | 45 ms | 3.44 ms |  | −92% (v1→v2) |
| Frame time (median) | Table, 100,000 rows | 112 ms | 6.88 ms |  | −94% (v1→v2) |
| Frame time (median) | Table, 100,000 rows, at the top | 111 ms | 6.90 ms |  | −94% (v1→v2) |
| Frame time (p95) | Table, 100,000 rows | 119 ms | 6.96 ms |  | −94% (v1→v2) |
| Frame time (median) | Table, 1,000 rows, visible rows only | n/a | 32.8 µs |  | −94% (v1 loop→v2) |
| Frame time (median) | Table, 100,000 rows, visible rows only | n/a | 32.9 µs |  | ÷3,400 (v1 loop→v2) |
| Frame time (median) | List, 1,000 rows | 478 µs | 78.0 µs |  | −84% (v1→v2) |
| Frame time (median) | List, 5,000 rows | 2.44 ms | 283 µs |  | −88% (v1→v2) |
| Frame time (median) | List, 50,000 rows | 44 ms | 2.57 ms |  | −94% (v1→v2) |
| Frame time (median) | List, 100,000 rows | 98 ms | 5.13 ms |  | −95% (v1→v2) |
| Frame time (median) | List, 100,000 rows, at the top | 100 ms | 5.11 ms |  | −95% (v1→v2) |
| Frame time (p95) | List, 100,000 rows | 105 ms | 5.18 ms |  | −95% (v1→v2) |
| Frame time (median) | List, 1,000 rows, visible rows only | n/a | 28.7 µs |  | −94% (v1 loop→v2) |
| Frame time (median) | List, 100,000 rows, visible rows only | n/a | 28.8 µs |  | ÷3,413 (v1 loop→v2) |
| Frame time (median) | OutlineView, 1,000 rows | 468 µs | 125 µs |  | −73% (v1→v2) |
| Frame time (median) | OutlineView, 5,000 rows | 2.39 ms | 553 µs |  | −77% (v1→v2) |
| Frame time (median) | OutlineView, 50,000 rows | 51 ms | 6.16 ms |  | −88% (v1→v2) |
| Frame time (median) | OutlineView, 100,000 rows | 112 ms | 15 ms |  | −86% (v1→v2) |
| Frame time (median) | OutlineView, 100,000 rows, at the top | 111 ms | 15 ms |  | −87% (v1→v2) |
| Frame time (p95) | OutlineView, 100,000 rows | 114 ms | 16 ms |  | −86% (v1→v2) |
| Frame time (median) | ColumnView, 1,000 rows | 418 µs | 114 µs |  | −73% (v1→v2) |
| Frame time (median) | ColumnView, 5,000 rows | 1.91 ms | 427 µs |  | −78% (v1→v2) |
| Frame time (median) | ColumnView, 50,000 rows | 34 ms | 3.98 ms |  | −88% (v1→v2) |
| Frame time (median) | ColumnView, 100,000 rows | 72 ms | 7.87 ms |  | −89% (v1→v2) |
| Frame time (median) | ColumnView, 100,000 rows, at the top | 72 ms | 7.91 ms |  | −89% (v1→v2) |
| Frame time (p95) | ColumnView, 100,000 rows | 74 ms | 7.96 ms |  | −89% (v1→v2) |
| Frame time (median) | ColumnView, 1,000 rows, visible rows only | n/a | 38.3 µs |  | −91% (v1 loop→v2) |
| Frame time (median) | ColumnView, 100,000 rows, visible rows only | n/a | 38.1 µs |  | ÷1,888 (v1 loop→v2) |

## Text

- **Scrolled labels**: a scroll view with 10,000 `Text` labels of which about 30 are visible.
- **Changing strings**: 50 labels whose text is different in every frame, like timers and the value next to a
  slider.
- **Many glyphs**: 72 lines in 24 sizes and three weights, the same in every frame: about 7,000 glyph quads out of
  a glyph cache of thousands.
- **Changing sizes**: one line whose size changes in every frame, cycling through 1,200 sizes. Every frame
  rasterizes about a hundred new glyphs and the atlas overflows several times per cycle; this is text being zoomed.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | Scrolled labels, at the top | 2.16 ms | 1.08 ms |  | −50% (v1→v2) |
| Frame time (median) | Scrolled labels, in the middle | 2.17 ms | 1.08 ms |  | −50% (v1→v2) |
| Frame time (p95) | Scrolled labels, in the middle | 2.57 ms | 1.13 ms |  | −56% (v1→v2) |
| Frame time (median) | Changing strings | 111 µs | 73.3 µs |  | −34% (v1→v2) |
| Frame time (p95) | Changing strings | 102 µs | 79.8 µs |  | −22% (v1→v2) |
| Frame time (median) | Many glyphs | 387 µs | 275 µs |  | −29% (v1→v2) |
| Frame time (median) | Changing sizes | 444 µs | 444 µs |  | unchanged (v1→v2) |
| Frame time (p95) | Changing sizes | 516 µs | 522 µs |  | +1% (v1→v2) |

## Layout

Stacks opened from one call site in a loop, without `PushID`: `for (...) { BeginHStack(); ...; EndHStack(); }`
inside a scroll view, each stack with one item. Stacks from one call site are told apart by their order.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | 100 stacks in a loop | 99.9 µs | 9.71 µs |  | −90% (v1→v2) |
| Frame time (median) | 1,000 stacks in a loop | 9.95 ms | 96.3 µs |  | −99% (v1→v2) |
| Frame time (median) | 10,000 stacks in a loop | 1.19 s | 981 µs |  | ÷1,213 (v1→v2) |

## Allocations

Calls of `operator new` per steady-state frame. Carbon's own frames are meant to make none; the Gallery pages that
do show some build `std::string`s in the example's page code (`Examples/Gallery`), which is counted along.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Allocations per frame | Table, 100,000 rows | 0 | 0 |  | unchanged (v1→v2) |
| Allocations per frame | Scrolled labels | 0 | 0 |  | unchanged (v1→v2) |
| Allocations per frame | Changing strings | 350 | 0 |  | −100% (v1→v2) |
| Allocations per frame | Changing sizes (new glyphs every frame) | 106 | 106 |  | unchanged (v1→v2) |
| Allocations per frame | 10,000 stacks in a loop | 0 | 0 |  | unchanged (v1→v2) |
| Allocations per frame | Line chart, 100,000 points | 0 | 0 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Typography page | 11 | 11 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Icons page | 2 | 2 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Buttons page | 1 | 1 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Sliders page | 1 | 1 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Layout page | 14 | 14 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Selection page | 4 | 4 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Dates page | 1 | 1 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Menus page | 2 | 2 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Toolbars page | 1 | 1 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Lists page | 1 | 1 |  | unchanged (v1→v2) |
| Allocations per frame | Gallery, Hierarchies page | 2 | 2 |  | unchanged (v1→v2) |

## Draw data

What a frame hands to the renderer backend: vertices (32 bytes each), indices (4 bytes), primitives, draw commands,
and the bytes of the glyph atlas that changed.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Vertices | Table, 100,000 rows | 4,404 | 4,404 |  | unchanged (v1→v2) |
| Indices | Table, 100,000 rows | 6,606 | 6,606 |  | unchanged (v1→v2) |
| Primitives | Table, 100,000 rows | 7 | 7 |  | unchanged (v1→v2) |
| Draw commands | Table, 100,000 rows | 3 | 3 |  | unchanged (v1→v2) |
| Draw data per frame | Table, 100,000 rows | 163.7 KB | 163.7 KB |  | unchanged (v1→v2) |
| Vertices | Gallery, Selection page | 3,492 | 3,492 |  | unchanged (v1→v2) |
| Indices | Gallery, Selection page | 5,238 | 5,238 |  | unchanged (v1→v2) |
| Primitives | Gallery, Selection page | 66 | 66 |  | unchanged (v1→v2) |
| Draw commands | Gallery, Selection page | 3 | 3 |  | unchanged (v1→v2) |
| Draw data per frame | Gallery, Selection page | 131.7 KB | 131.7 KB |  | unchanged (v1→v2) |
| Vertices | Line chart, 1,000 points | 4,068 | 4,068 |  | unchanged (v1→v2) |
| Indices | Line chart, 1,000 points | 6,102 | 6,102 |  | unchanged (v1→v2) |
| Primitives | Line chart, 1,000 points | 1,002 | 1,002 |  | unchanged (v1→v2) |
| Draw commands | Line chart, 1,000 points | 2 | 2 |  | unchanged (v1→v2) |
| Draw data per frame | Line chart, 1,000 points | 182.3 KB | 182.3 KB |  | unchanged (v1→v2) |
| Vertices | Line chart, 100,000 points | 400,068 | 9,772 |  | −98% (v1→v2) |
| Indices | Line chart, 100,000 points | 600,102 | 14,658 |  | −98% (v1→v2) |
| Primitives | Line chart, 100,000 points | 100,002 | 2,428 |  | −98% (v1→v2) |
| Draw commands | Line chart, 100,000 points | 2 | 2 |  | unchanged (v1→v2) |
| Draw data per frame | Line chart, 100,000 points | 17.6 MB | 438.6 KB |  | −98% (v1→v2) |
| Atlas upload per frame | Changing sizes | 324.6 KB | 324.6 KB |  | unchanged (v1→v2) |
| Atlas clears per 1,000 frames | Changing sizes | 4.21 | 4.20 |  | unchanged (v1→v2) |
| Atlas upload per frame | Many glyphs (steady state) | 0 B | 0 B |  | unchanged (v1→v2) |

## Backends

CPU time of one call of the backend's render function. "New frame" follows a `NewFrame`/`EndFrame` pair; "redraw"
renders the same frame again. The small scene is the renderer tests' scene (85 quads in 4 draw commands), the large
one a window full of text (10,621 quads in one command). The harnesses run each API the way the tests do, with its
validation or debug layer where one is installed (Vulkan, Direct3D 11, OpenGL debug output), so the absolute
numbers of those include it; Vulkan's vary by some ten percent from run to run and were measured a second time for
v2. "State queries" are the `glGet*` and `glIsEnabled` calls of one render call, which the harness counts from v2
on; the v1 counts are those of the v1 code, which queried the same state for a redraw as for a new frame.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Render time (median) | WebGPU, small scene, new frame | 15.2 µs | 14.9 µs |  | −2% (v1→v2) |
| Render time (median) | WebGPU, small scene, redraw | 14.6 µs | 6.83 µs |  | −53% (v1→v2) |
| Render time (median) | WebGPU, large scene, new frame | 96.6 µs | 59.5 µs |  | −38% (v1→v2) |
| Render time (median) | WebGPU, large scene, redraw | 95.7 µs | 7.81 µs |  | −92% (v1→v2) |
| Render time (median) | Vulkan, small scene, new frame | 44.7 µs | 41.3 µs |  | −8% (v1→v2) |
| Render time (median) | Vulkan, small scene, redraw | 37.6 µs | 39.7 µs |  | +6% (v1→v2) |
| Render time (median) | Vulkan, large scene, new frame | 141 µs | 141 µs |  | unchanged (v1→v2) |
| Render time (median) | Vulkan, large scene, redraw | 81.8 µs | 80.6 µs |  | −1% (v1→v2) |
| Render time (median) | OpenGL, small scene, new frame | 8.69 µs | 6.27 µs |  | −28% (v1→v2) |
| Render time (median) | OpenGL, small scene, redraw | 10.3 µs | 3.04 µs |  | −71% (v1→v2) |
| Render time (median) | OpenGL, large scene, new frame | 81.1 µs | 65.7 µs |  | −19% (v1→v2) |
| Render time (median) | OpenGL, large scene, redraw | 81.2 µs | 6.77 µs |  | −92% (v1→v2) |
| Render time (median) | OpenGLES, small scene, new frame | 6.96 µs | 6.26 µs |  | −10% (v1→v2) |
| Render time (median) | OpenGLES, small scene, redraw | 6.33 µs | 3.85 µs |  | −39% (v1→v2) |
| Render time (median) | OpenGLES, large scene, new frame | 84.1 µs | 59.5 µs |  | −29% (v1→v2) |
| Render time (median) | OpenGLES, large scene, redraw | 87.0 µs | 7.80 µs |  | −91% (v1→v2) |
| Render time (median) | DX11, small scene, new frame | 22.2 µs | 20.3 µs |  | −8% (v1→v2) |
| Render time (median) | DX11, small scene, redraw | 22.3 µs | 17.0 µs |  | −23% (v1→v2) |
| Render time (median) | DX11, large scene, new frame | 89.0 µs | 58.9 µs |  | −34% (v1→v2) |
| Render time (median) | DX11, large scene, redraw | 77.9 µs | 20.4 µs |  | −74% (v1→v2) |
| Render time (median) | DX9, small scene, new frame | 37.8 µs | 33.3 µs |  | −12% (v1→v2) |
| Render time (median) | DX9, small scene, redraw | 37.4 µs | 13.6 µs |  | −64% (v1→v2) |
| Render time (median) | DX9, large scene, new frame | 293 µs | 275 µs |  | −6% (v1→v2) |
| Render time (median) | DX9, large scene, redraw | 270 µs | 28.2 µs |  | −90% (v1→v2) |
| State queries per render | OpenGL, new frame | 36 | 32 |  | −11% (v1→v2) |
| State queries per render | OpenGL, redraw | 36 | 25 |  | −31% (v1→v2) |
| State queries per render | OpenGL ES, new frame | 30 | 27 |  | −10% (v1→v2) |
| State queries per render | OpenGL ES, redraw | 30 | 21 |  | −30% (v1→v2) |

## Idle

How many frames an event-driven host renders while nothing happens. The simulated host renders a frame while
`IsAnimating()` is true and otherwise sleeps until the next event (v1), or, from v2 on, sleeps for
`GetNextFrameDelay()` seconds and renders then. "Frame time" here is the CPU time of all frames of the simulated
period.

- **Focused text field**: a form with one focused text field and no input, for ten seconds. Only the caret blinks.
- **Static interface**: the same form without focus; the one frame is the one the host renders to begin with.
- **After scrolling**: one notch of the mouse wheel over a scroll view, then nothing for three seconds: the frames
  until the scroll indicator has faded out again.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frames per second | Focused text field | 60 | 2.10 |  | −96% (v1→v2) |
| CPU time per 10 s | Focused text field | 1.08 ms | 30.0 µs |  | −97% (v1→v2) |
| Frames per second | Static interface | 0.10 | 0.10 |  | unchanged (v1→v2) |
| Frames per wheel notch | After scrolling | 75 | 31.1 |  | −59% (v1→v2) |
| CPU time per wheel notch | After scrolling | 3.61 ms | 825 µs |  | −77% (v1→v2) |

## Charts and controls

- **Line chart**: one series of 1,000 to 100,000 values in a plot about 1,200 points wide, without and with a label
  per value.
- **PopUpButton**, closed, with 10 to 1,000 items; **SegmentedControl** with 4 and 12 segments.
- **ComboBox** with its list open, showing all of 100 to 10,000 items, or the ones that match a typed "9".

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | Line chart, 1,000 points | 35.3 µs | 25.9 µs |  | −27% (v1→v2) |
| Frame time (median) | Line chart, 1,000 points, with labels | 81.4 µs | 36.9 µs |  | −55% (v1→v2) |
| Frame time (median) | Line chart, 10,000 points | 333 µs | 68.9 µs |  | −79% (v1→v2) |
| Frame time (median) | Line chart, 10,000 points, with labels | 802 µs | 80.6 µs |  | −90% (v1→v2) |
| Frame time (median) | Line chart, 100,000 points | 3.36 ms | 133 µs |  | −96% (v1→v2) |
| Frame time (median) | Line chart, 100,000 points, with labels | 29 ms | 143 µs |  | ÷205 (v1→v2) |
| Frame time (median) | PopUpButton, 10 items | 1.62 µs | 0.94 µs |  | −42% (v1→v2) |
| Frame time (median) | PopUpButton, 100 items | 5.15 µs | 1.25 µs |  | −76% (v1→v2) |
| Frame time (median) | PopUpButton, 1,000 items | 47.6 µs | 4.20 µs |  | −91% (v1→v2) |
| Frame time (median) | SegmentedControl, 4 segments | 3.14 µs | 1.97 µs |  | −37% (v1→v2) |
| Frame time (median) | SegmentedControl, 12 segments | 8.42 µs | 4.94 µs |  | −41% (v1→v2) |
| Frame time (median) | ComboBox open, 100 items | 16.6 µs | 4.34 µs |  | −74% (v1→v2) |
| Frame time (median) | ComboBox open, 100 items, filtered | 10.2 µs | 4.26 µs |  | −58% (v1→v2) |
| Frame time (median) | ComboBox open, 1,000 items | 114 µs | 4.33 µs |  | −96% (v1→v2) |
| Frame time (median) | ComboBox open, 1,000 items, filtered | 56.6 µs | 4.26 µs |  | −92% (v1→v2) |
| Frame time (median) | ComboBox open, 10,000 items | 1.24 ms | 4.35 µs |  | ÷285 (v1→v2) |
| Frame time (median) | ComboBox open, 10,000 items, filtered | 634 µs | 4.26 µs |  | −99% (v1→v2) |

## Full frames

Whole frames of the Gallery example at its window size: sidebar, header and one page, built by the example's own
page code. These are the frames an ordinary application has.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | Gallery, Typography page | 39.3 µs | 23.7 µs |  | −40% (v1→v2) |
| Frame time (median) | Gallery, Icons page | 29.6 µs | 18.2 µs |  | −38% (v1→v2) |
| Frame time (median) | Gallery, Buttons page | 35.0 µs | 22.2 µs |  | −36% (v1→v2) |
| Frame time (median) | Gallery, Toggles page | 29.9 µs | 18.4 µs |  | −39% (v1→v2) |
| Frame time (median) | Gallery, Sliders page | 22.9 µs | 14.6 µs |  | −36% (v1→v2) |
| Frame time (median) | Gallery, TextFields page | 27.3 µs | 16.3 µs |  | −40% (v1→v2) |
| Frame time (median) | Gallery, Layout page | 57.2 µs | 34.0 µs |  | −41% (v1→v2) |
| Frame time (median) | Gallery, Selection page | 83.7 µs | 51.4 µs |  | −39% (v1→v2) |
| Frame time (median) | Gallery, Dates page | 107 µs | 73.9 µs |  | −31% (v1→v2) |
| Frame time (median) | Gallery, Menus page | 52.7 µs | 29.6 µs |  | −44% (v1→v2) |
| Frame time (median) | Gallery, Toolbars page | 62.9 µs | 41.4 µs |  | −34% (v1→v2) |
| Frame time (median) | Gallery, Lists page | 52.2 µs | 29.4 µs |  | −44% (v1→v2) |
| Frame time (median) | Gallery, Hierarchies page | 68.7 µs | 39.2 µs |  | −43% (v1→v2) |
| Frame time (median) | Gallery, Navigation page | 40.4 µs | 22.8 µs |  | −44% (v1→v2) |
| Frame time (median) | Gallery, Charts page | 34.4 µs | 21.2 µs |  | −38% (v1→v2) |
| Frame time (median) | Gallery, Reflection page | 70.7 µs | 41.3 µs |  | −42% (v1→v2) |

## Build

All targets (the three libraries, every example once per backend, the tests and the benchmarks) with all six
renderer backends, built with Ninja on 16 threads.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Clean build (Debug) | all targets | 86.3 s | --- | 68.8 s | −20% (v1→v3) |
| Clean build, CPU seconds (Debug) | all targets | 1345 s | --- | 953 s | −29% (v1→v3) |
| Configure (Debug) | empty build directory | 8.3 s | --- | 8.3 s | unchanged (v1→v3) |
| Rebuild (Debug) | after touching `Core/Log.h` | 49.2 s | --- | 40.1 s | −18% (v1→v3) |
| Rebuild, CPU seconds (Debug) | after touching `Core/Log.h` | 823 s | --- | 659 s | −20% (v1→v3) |
| Rebuild (Debug) | after changing the project version | 15.6 s | --- | 3.8 s | −75% (v1→v3) |
| Clean build (Release) | all targets | 96.0 s | --- | 74.2 s | −23% (v1→v3) |
| Clean build, CPU seconds (Release) | all targets | 1679 s | --- | 1168 s | −30% (v1→v3) |
| Configure (Release) | empty build directory | 8.2 s | --- | 8.2 s | unchanged (v1→v3) |
| Rebuild (Release) | after touching `Core/Log.h` | 67.6 s | --- | 53.7 s | −21% (v1→v3) |
| Rebuild, CPU seconds (Release) | after touching `Core/Log.h` | 1178 s | --- | 925 s | −21% (v1→v3) |
| Rebuild (Release) | after changing the project version | 17.6 s | --- | 6.2 s | −65% (v1→v3) |

## Tests

`ctest` on the Release build, as CI runs it: one test at a time in v1, with `--parallel 4` in v3. "One at a time"
is the suite of v3 without `--parallel`, for comparison. The backends' tests are the entries whose names start with
`Backends` (one per test in v1, one per backend in v3): the renderer tests, the smoke tests and the comparison with
WebGPU, once per backend. On this machine every backend has a device, so none of them is skipped.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| `ctest` wall time | whole suite | 31.2 s | --- | 5.9 s | −81% (v1→v3) |
| `ctest` wall time | whole suite, one at a time | 31.2 s | --- | 15.5 s | −50% (v1→v3) |
| `ctest` wall time | the backends' tests | 20.7 s | --- | 5.0 s | −76% (v1→v3) |
| `ctest` entries | whole suite | 703 | --- | 610 | −13% (v1→v3) |

## CI

Duration of each job of a green run of `.github/workflows/CI.yml` on `main`, from GitHub's API. v1 is the run of
`b72ce8f`, v3 the run of `66272b9`, the first with the new workflow: its caches were empty, so it installed the
Emscripten and Vulkan SDKs once more and compiled everything through an empty ccache, and it shows the effect of
less compile work, parallel tests and the single package check, not yet that of the caches. No further run was
made, because CI was then paused to save minutes. The runners vary: the three runs before the CI changes took 686,
714 and 732 s for the Windows job, 362, 272 and 478 s for Linux Clang, 258, 205 and 307 s for Linux GCC and 398,
238 and 296 s for Emscripten, so single jobs moving by a third either way is noise; the Windows build step, 465 s
in the last run before and 265 s after, is not.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Job duration | clang-format | 7 s | --- | 11 s | +57% (v1→v3) |
| Job duration | Dawn (Windows) | 5 s | --- | 8 s | +60% (v1→v3) |
| Job duration | Dawn (Linux) | 4 s | --- | 6 s | +50% (v1→v3) |
| Job duration | Emscripten (WebGL 2) | 398 s | --- | 188 s | −53% (v1→v3) |
| Job duration | Windows MSVC (Release) | 686 s | --- | 460 s | −33% (v1→v3) |
| Job duration | Linux GCC (Debug) | 258 s | --- | 224 s | −13% (v1→v3) |
| Job duration | Linux Clang (Release) | 362 s | --- | 413 s | +14% (v1→v3) |
| Run duration | all jobs, wall clock | 697 s | --- | 477 s | −32% (v1→v3) |

## Changes

### v1

The baseline. What it shows:

- **Rows are the largest cost.** A frame is linear in the number of rows, about 1.1 µs per row for a table: 0.56 ms
  at 1,000 rows and 111 ms at 100,000, wherever the view is scrolled to. Every row is laid out, hit-tested, has its
  state looked up and its text shaped, visible or not, although the draw data is that of the 30 visible rows either
  way.
- **Stacks in a loop are quadratic.** 100 stacks take 0.1 ms, 1,000 take 10 ms and 10,000 take 1.2 s: every stack
  probes the IDs of all the stacks before it from the same call site.
- **Text out of view is not free.** 10,000 labels of which 30 are visible take 2.2 ms. Changing strings allocate 7
  times per string and frame.
- **Charts draw what cannot be seen.** A line chart draws one quad per value: 100,000 points are 18 MB of draw data
  for a plot 1,200 points wide, and a label per value costs another 26 ms for measuring all of them.
- **Backends upload a frame every time it is rendered.** Rendering a frame again costs as much as rendering a new
  one everywhere but on Vulkan.
- **A focused text field keeps the host awake**: 60 frames per second for a caret that changes twice a second, and
  75 frames follow one notch of the mouse wheel.
- **Ordinary frames are fast.** The Gallery's pages take 23 to 107 µs.

### v2: runtime

- **Stacks in a loop are counted, not probed.** Stacks begun from one call site in a frame are told apart by their
  order. Finding the position of a new one probed the records of all the ones before it, n²/2 state lookups for n
  stacks; the first stack's record now counts them. 10,000 stacks: 1.19 s → 1.55 ms. (`8892da2`)
- **Per-ID state in an open-addressing table over pooled blocks.** A state block was a node of an `unordered_map`
  plus a heap allocation of its own, and every block was visited at the end of every frame. Blocks now live in
  chunks that never move, are found by linear probing, and return to a free list by size, so state that comes and
  goes allocates nothing; only transient blocks are visited at the end of a frame. On top of the previous change:
  10,000 stacks 1.55 → 1.26 ms, OutlineView with 100,000 rows 112 → 90 ms. (`f698acf`)
- **Rows out of view do no work, and applications can skip them.** A row of Sidebar, List, Table, OutlineView or
  ColumnView that is scrolled out of view still takes its space and its place in the selection, but is not
  hit-tested, hashed, shaped or drawn, and the components look their scratch state up once per frame instead of
  once per row and cell. `ClipTableRows`, `ClipListItems` and `ClipColumnViewItems` return the rows an application
  has to submit at all. Table with 100,000 rows: 112 ms → 6.9 ms with the unchanged loop and 59 µs with the range
  (33 µs after the later changes), the same as for 1,000 rows. (`54d9d87`)
- **Text outside the clip rectangle is rejected before it is shaped.** `TextSystem::Draw` hashed, shaped and
  line-broke every text before testing each line against the clip rectangle. Text below the clip rectangle, or
  above it when its line count is known, now returns at once, and the `Text` widget skips drawing a measured
  paragraph that is out of view. 10,000 labels: 2.17 → 1.58 ms. (`934da62`)
- **Hashing eight bytes per step.** `HashBytes` was FNV-1a, one multiplication per byte on one dependency chain,
  and `HashCombine` ran eight such rounds per integer; every widget ID, state lookup and shaped-line lookup goes
  through them. `HashBytes` now reads eight bytes at a time and `HashCombine` is a two-multiplication mixer. Every
  Gallery page got 10 to 17 % faster from this alone; 10,000 labels 1.58 → 1.16 ms. (`a6347a0`)
- **The cache of shaped lines is bounded and recycles its entries.** Every new string added a map node and a glyph
  vector (seven allocations) that stayed for ten seconds. The cache now keeps its lines in the order of their last
  use; beyond 1,024 lines a new line takes the node and the storage of the least recently used one, unless that was
  used in the current frame. 50 changing strings: 350 → 0 allocations per frame, 106 → 76 µs. (`0e75ec9`)
- **A shaped line remembers where it found its glyphs.** Drawing a line looked every glyph up in the glyph cache by
  a hashed key, every frame. A line now keeps the cache entry of each glyph with the pixel size and sub-pixel
  position it is valid for. Gallery pages 9 to 13 % faster on top of the previous changes. (`85fc92f`)
- **Dense line charts are drawn per pixel column, and only drawn labels are measured.** A line chart drew a quad
  per value and measured every label to choose which to draw. With more than two values per pixel column it draws
  one stroke per column over the range of that column's values; the label stride is found by measuring only the
  labels a stride selects. 100,000 labelled points: 29.4 ms → 164 µs, draw data 18.4 MB → 0.45 MB. Charts with
  fewer values are drawn exactly as before. (`ad06d3c`)
- **A combo box list builds its visible rows and filters once per text.** The open list matched every item against
  the text twice per frame and built a row for each match, though it shows eight. Matches are found when the text
  or the items change, and only the rows in view are built. 10,000 items: 1.24 ms → 5.5 µs. (`0accb67`)
- **Hosts can sleep until the next frame is due.** A focused text field set `IsAnimating()` in every frame for its
  caret, and a scroll view did for the second its indicator stays. `RequestFrameAfter(seconds)` schedules a frame
  instead and `GetNextFrameDelay()` tells the host how long it may wait. So that a sleep does not swallow an
  animation, one that starts in a frame after stillness advances by at most 1/30 s in that frame. Focused text
  field: 60 → 2 frames per second; one wheel notch: 75 → 33 frames. (`152aca9`)
- **Backends upload a frame's geometry once.** WebGPU, Direct3D 11, Direct3D 9 and OpenGL / OpenGL ES wrote all
  vertices, indices and primitives on every `Render` call (Direct3D 9 also copied the primitives into the vertices
  each time). They now upload in the first `Render` after `EndFrame`, as Vulkan did. Rendering the large scene
  again: WebGPU 96 → 7.7 µs, OpenGL 81 → 8.2 µs, Direct3D 11 78 → 23 µs, Direct3D 9 270 → 26 µs. (`0dc8e2d`)
- **OpenGL queries less state.** `OpenGLRender` saved state it never changes (the depth write mask, the unpack
  state for 3D images and bitmaps) and the array buffer binding also when nothing was uploaded. State queries per
  render call, each a round trip in a browser: OpenGL ES / WebGL 2 30 → 27 for a new frame and 30 → 21 for a
  redraw; OpenGL 36 → 32 and 36 → 25. The harness of the renderer tests counts them. (`50a9fe0`)
- **Quads are written in place.** `AddQuad` appended four vertices with `push_back` and six indices with `insert`,
  each with its own capacity check, for every shape and glyph. It now grows both arrays once and writes into them.
  Gallery pages another 13 to 20 % faster; a clipped table 45 → 33 µs. (`9f0e689`)
- **A pop-up button remembers the width of its widest item.** It measured every item in every frame to size itself.
  The width is kept with a hash of the items and the font. 10 items: 1.17 → 0.94 µs, 1,000 items: 38 → 4.2 µs.
  (`0e4f99f`)

### v3: build and CI

- **HarfBuzz is built in batches of twelve sources.** HarfBuzz was some seventy translation units that each include
  most of the library: nearly a third of the compile time of a clean build. It is written to be compiled in one
  piece, so a unity build applies. Building its target alone: 245 → 36 CPU seconds, 19.3 → 13.2 s wall clock.
  (`44e2d8c`)
- **The project version reaches only `Version.cpp`.** The `CARBON_VERSION_*` definitions were on the whole `Carbon`
  target, so a new version recompiled the library. Rebuild after a version change: 17.6 → 6.2 s in Release, 15.6 →
  3.8 s in Debug; what is left is CMake running again and every executable linking again. (`122a97b`)
- **The examples' sources are compiled once, not once per backend.** Every example exists once per backend, and its
  backend-neutral sources were compiled for each, as were `Example::App` and `Example::Host`. They are compiled
  once now, into object libraries and `CarbonExampleBase`; a `<Backend><Example>` executable links them with the
  device of its backend. Compile steps of those sources with six backends: 77 → 22. (`784a155`)
- **The renderer tests share a device and run as one CTest entry per backend.** Every test was a process of its
  own, and every renderer test created an instance and a device; the comparison with WebGPU rendered the reference
  again each time. Tests of a process now share one harness and device per backend, CTest runs the renderer tests
  per backend, and everything else is safe for `ctest --parallel`. `Backends` tests: 20.7 → 5.0 s; whole suite 31.2
  → 15.5 s one at a time, 5.9 s with `--parallel 4`. (`7861d6c`)
- **CI: parallel tests, compiler cache, cached tools, one package check.** `ctest --parallel 4` in every job;
  `ccache` with a cache kept between runs in the Linux jobs; the Emscripten SDK and the needed files of the Vulkan
  SDK (now pinned to a version) cached; the installed-package check in the Linux Clang job only. First run with it,
  caches still empty: the Windows job 732 → 460 s (its build step 465 → 265 s, tests 17 → 8 s, no package check),
  Emscripten tests 40 → under 4 s. (`66272b9`)

### Tried and rejected

- **WebGPU: skip binding the texture's bind group when it has not changed.** Render time of the small scene (4 draw
  commands, 3 redundant binds), frame rendered again: 6.37 → 6.51 µs. No gain: `SetBindGroup` costs too little next
  to the rest of a draw command. Reverted.
- **Glyph atlas: dirty rectangle with an X extent instead of full-width rows.** Measured before any backend was
  touched, with the atlas reporting the extent and the benchmark's backend counting what an upload would be: 332 →
  327 KB per frame (−2 %) in the changing-sizes scenario, because the new glyphs of a frame are spread over the
  width of the atlas. Steady-state frames upload 0 bytes either way. Reverted.
- **SegmentedControl: remember the width of the widest segment.** 4 segments 1.98 → 1.91 µs (−4 %), 12 segments
  4.94 → 4.66 µs (−6 %): below the bar. The control draws every segment's text anyway, so measuring it is one more
  lookup of a line it shapes regardless. Reverted for SegmentedControl, kept for PopUpButton, which draws one of
  its items.
- **Starting every animation with a short first step.** The first version of the rule for sleeping hosts limited
  the frame in which any animation starts to 1/30 s. Three animation tests that step coarsely failed, rightly: a
  host that renders continuously at a low frame rate would see every animation start late. The rule now applies
  only after a frame in which nothing moved.
- **HarfBuzz in one translation unit.** A unity build without batches (HarfBuzz's own `harfbuzz.cc` arrangement)
  costs about as many CPU seconds as batches of twelve (34 against 36) but 18.8 s of wall clock against 13.2 s,
  because nothing else of it compiles in parallel. Batches kept.

### Not significant

Findings of the survey that the baseline did not confirm as a cost worth changing.

- **Glyph atlas eviction instead of clearing.** The atlas overflows only in the scenario built to make it overflow
  (a text whose size changes every frame): 4.2 clears per 1,000 frames there, 0 clears and 0 upload bytes per frame
  in every other scenario. Left as it is.
- **Primitive deduplication beyond the previous primitive.** A Gallery frame has 12 to 66 primitives next to 1,200
  to 4,000 vertices; the 100,000-row table has 7. Primitives are under 2 % of the draw data.
- **Collapsing deferred container backgrounds that end up off screen.** No scenario has enough of them to show in a
  number: a Gallery frame has a handful of containers with a background.
- **Measuring and drawing `Text` in one pass.** After the hash and the glyph slots, a label out of view costs about
  110 ns (10,000 labels in 1.13 ms), most of it layout; the second lookup of a visible label's shaped line is a few
  nanoseconds of a frame of 20 to 70 µs. Not tried.
- **Vulkan: one submit for atlas uploads, unchanged scissors skipped.** The atlas is uploaded only when glyphs are
  new, and a frame has 3 to 13 draw commands. Not tried. Vulkan's render call is the slowest of the six for the
  small scene (45 µs), which is worth a look of its own.
- **`needs: dawn` makes every build job wait for both Dawn jobs.** With the Dawn caches warm, both Dawn jobs take 4
  to 5 s and the build jobs are created 7 s after the run starts: 1 % of the 714 s of the longest job. Left as it
  is.
- **The 228 KB generated `Icons.h` in the `Extension.h` umbrella.** Compiling a file that includes only `Icons.h`
  takes as long as one that includes only `<string_view>` (1.33 to 1.39 s against 1.48 to 1.50 s with the
  compiler's start-up, the difference is noise): a few thousand `constexpr` strings are cheap to parse. Left in the
  umbrella.
- **Faster font embedding (`EmbedAsset.cmake`).** All generated assets together are 18 build steps and 11 of the
  1,680 CPU seconds of a clean Release build.
- **Parallel clang-format in `Scripts/Format.sh`.** The check over all sources takes 1.0 s, and the CI job 7 to 8
  s, most of it installing clang-format. On Windows the script had to learn to format in batches for another
  reason: the command line was too long.
- **Caching apt packages in CI.** Installing them takes 17 to 18 s in each Linux job. Caching them needs a
  third-party action; not done.
- **The effect of the CI caches.** ccache on Linux, the cached Emscripten SDK and the cached Vulkan SDK files only
  pay from the second run on. That run was not made: CI was paused after the first one to save minutes. The first
  run stored the caches.
- **A compiler cache for the Windows job, and precompiled headers.** Not tried in this round. The Windows build
  step is the longest in CI; ccache and sccache both need care with MSVC's flags, which cannot be tested without a
  series of CI runs.
