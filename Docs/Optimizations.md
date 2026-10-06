# Optimizations

What Carbon costs at run time and at build time, measured before and after each round of optimization. Three
versions are compared:

- **v1**: the baseline, commit `3f9a41d`, before any optimization.
- **v2**: after the runtime optimizations (not yet recorded).
- **v3**: after the build and CI optimizations (not yet recorded).

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
  offscreen harnesses of the renderer tests (`Tests/src/Support/*Harness.cpp`). Creating the target and reading
  it back are not part of the time. A backend whose device cannot be created is skipped with a message. Two
  scenes (the renderer tests' scene with 85 quads, and a window full of text with 10,621 quads), each rendered
  after a new frame and rendered again without one, as a host does that redraws a window.

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

Build and test costs come from `Scripts/BuildMetrics.py`, run from a Visual Studio developer prompt on an
otherwise idle machine:

```sh
python Scripts/BuildMetrics.py --dawn-debug <dawn>/install/Debug --dawn-release <dawn>/install/Release --out Scratch/BuildMetrics.json
```

It configures an empty build directory with Ninja and builds all targets, three times per configuration; rebuilds
after touching `Framework/src/Carbon/Core/Log.h` and after changing the project version; runs `ctest`; and reads
the job durations of the latest green CI run on `main` from GitHub's public API. "CPU seconds" are the durations
of all build steps in `.ninja_log` added up: what the build would take on one core.

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

CI durations are those of GitHub's hosted runners (`windows-2022`, `ubuntu-24.04`) and vary from run to run by
some ten percent.

### The keep rule

An optimization stays only if

1. its metric improves by at least 10 % (or allocations per frame drop to 0),
2. all tests pass in Debug and Release with zero warnings in Carbon's targets, and
3. `python Scripts/Screenshots.py --build Build/Release --check` reports the documentation images unchanged.

Otherwise it is reverted and listed under [Tried and rejected](#tried-and-rejected) with what was measured.

In the tables, the change is relative to v1. Runtime rows leave v3 empty unless they were measured again; build
rows show `---` under v2, which does not touch them.

## Rows

Table, List, OutlineView and ColumnView in a view that shows about 30 rows of 24 points, with 1,000 to 100,000 rows
and one row selected. The application submits every row in a plain loop, as the components' documentation shows
(`for (i...) TableRow(i)`). The table has three text columns, the list items
have an icon and a detail text, the outline view is folders of nine files each, all expanded, and the column view
has one long column and a second one of 30 items. "At the top" is scrolled to the first row, everything else to
the middle of the rows.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | Table, 1,000 rows | 565 µs |  |  |  |
| Frame time (median) | Table, 5,000 rows | 2.75 ms |  |  |  |
| Frame time (median) | Table, 50,000 rows | 45 ms |  |  |  |
| Frame time (median) | Table, 100,000 rows | 112 ms |  |  |  |
| Frame time (median) | Table, 100,000 rows, at the top | 111 ms |  |  |  |
| Frame time (p95) | Table, 100,000 rows | 119 ms |  |  |  |
| Frame time (median) | List, 1,000 rows | 478 µs |  |  |  |
| Frame time (median) | List, 5,000 rows | 2.44 ms |  |  |  |
| Frame time (median) | List, 50,000 rows | 44 ms |  |  |  |
| Frame time (median) | List, 100,000 rows | 98 ms |  |  |  |
| Frame time (median) | List, 100,000 rows, at the top | 100 ms |  |  |  |
| Frame time (p95) | List, 100,000 rows | 105 ms |  |  |  |
| Frame time (median) | OutlineView, 1,000 rows | 468 µs |  |  |  |
| Frame time (median) | OutlineView, 5,000 rows | 2.39 ms |  |  |  |
| Frame time (median) | OutlineView, 50,000 rows | 51 ms |  |  |  |
| Frame time (median) | OutlineView, 100,000 rows | 112 ms |  |  |  |
| Frame time (median) | OutlineView, 100,000 rows, at the top | 111 ms |  |  |  |
| Frame time (p95) | OutlineView, 100,000 rows | 114 ms |  |  |  |
| Frame time (median) | ColumnView, 1,000 rows | 418 µs |  |  |  |
| Frame time (median) | ColumnView, 5,000 rows | 1.91 ms |  |  |  |
| Frame time (median) | ColumnView, 50,000 rows | 34 ms |  |  |  |
| Frame time (median) | ColumnView, 100,000 rows | 72 ms |  |  |  |
| Frame time (median) | ColumnView, 100,000 rows, at the top | 72 ms |  |  |  |
| Frame time (p95) | ColumnView, 100,000 rows | 74 ms |  |  |  |

## Text

- **Scrolled labels**: a scroll view with 10,000 `Text` labels of which about 30 are visible.
- **Changing strings**: 50 labels whose text is different in every frame, like timers and the value next to a
  slider.
- **Many glyphs**: 72 lines in 24 sizes and three weights, the same in every frame: about 7,000 glyph quads out
  of a glyph cache of thousands.
- **Changing sizes**: one line whose size changes in every frame, cycling through 1,200 sizes. Every frame
  rasterizes about a hundred new glyphs and the atlas overflows several times per cycle; this is text being zoomed.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | Scrolled labels, at the top | 2.16 ms |  |  |  |
| Frame time (median) | Scrolled labels, in the middle | 2.17 ms |  |  |  |
| Frame time (p95) | Scrolled labels, in the middle | 2.57 ms |  |  |  |
| Frame time (median) | Changing strings | 111 µs |  |  |  |
| Frame time (p95) | Changing strings | 102 µs |  |  |  |
| Frame time (median) | Many glyphs | 387 µs |  |  |  |
| Frame time (median) | Changing sizes | 444 µs |  |  |  |
| Frame time (p95) | Changing sizes | 516 µs |  |  |  |

## Layout

Stacks opened from one call site in a loop, without `PushID`: `for (...) { BeginHStack(); ...; EndHStack(); }`
inside a scroll view, each stack with one item. Stacks from one call site are told apart by their order.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | 100 stacks in a loop | 99.9 µs |  |  |  |
| Frame time (median) | 1,000 stacks in a loop | 9.95 ms |  |  |  |
| Frame time (median) | 10,000 stacks in a loop | 1.19 s |  |  |  |

## Allocations

Calls of `operator new` per steady-state frame. Carbon's own frames are meant to make none; the Gallery pages that
do show some build `std::string`s in the example's page code (`Examples/Gallery`), which is counted along.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Allocations per frame | Table, 100,000 rows | 0 |  |  |  |
| Allocations per frame | Scrolled labels | 0 |  |  |  |
| Allocations per frame | Changing strings | 350 |  |  |  |
| Allocations per frame | Changing sizes (new glyphs every frame) | 106 |  |  |  |
| Allocations per frame | 10,000 stacks in a loop | 0 |  |  |  |
| Allocations per frame | Line chart, 100,000 points | 0 |  |  |  |
| Allocations per frame | Gallery, Typography page | 11 |  |  |  |
| Allocations per frame | Gallery, Icons page | 2 |  |  |  |
| Allocations per frame | Gallery, Buttons page | 1 |  |  |  |
| Allocations per frame | Gallery, Sliders page | 1 |  |  |  |
| Allocations per frame | Gallery, Layout page | 14 |  |  |  |
| Allocations per frame | Gallery, Selection page | 4 |  |  |  |
| Allocations per frame | Gallery, Dates page | 1 |  |  |  |
| Allocations per frame | Gallery, Menus page | 2 |  |  |  |
| Allocations per frame | Gallery, Toolbars page | 1 |  |  |  |
| Allocations per frame | Gallery, Lists page | 1 |  |  |  |
| Allocations per frame | Gallery, Hierarchies page | 2 |  |  |  |

## Draw data

What a frame hands to the renderer backend: vertices (32 bytes each), indices (4 bytes), primitives, draw commands,
and the bytes of the glyph atlas that changed.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Vertices | Table, 100,000 rows | 4,404 |  |  |  |
| Indices | Table, 100,000 rows | 6,606 |  |  |  |
| Primitives | Table, 100,000 rows | 7 |  |  |  |
| Draw commands | Table, 100,000 rows | 3 |  |  |  |
| Draw data per frame | Table, 100,000 rows | 163.7 KB |  |  |  |
| Vertices | Gallery, Selection page | 3,492 |  |  |  |
| Indices | Gallery, Selection page | 5,238 |  |  |  |
| Primitives | Gallery, Selection page | 66 |  |  |  |
| Draw commands | Gallery, Selection page | 3 |  |  |  |
| Draw data per frame | Gallery, Selection page | 131.7 KB |  |  |  |
| Vertices | Line chart, 1,000 points | 4,068 |  |  |  |
| Indices | Line chart, 1,000 points | 6,102 |  |  |  |
| Primitives | Line chart, 1,000 points | 1,002 |  |  |  |
| Draw commands | Line chart, 1,000 points | 2 |  |  |  |
| Draw data per frame | Line chart, 1,000 points | 182.3 KB |  |  |  |
| Vertices | Line chart, 100,000 points | 400,068 |  |  |  |
| Indices | Line chart, 100,000 points | 600,102 |  |  |  |
| Primitives | Line chart, 100,000 points | 100,002 |  |  |  |
| Draw commands | Line chart, 100,000 points | 2 |  |  |  |
| Draw data per frame | Line chart, 100,000 points | 17.6 MB |  |  |  |
| Atlas upload per frame | Changing sizes | 324.6 KB |  |  |  |
| Atlas clears per 1,000 frames | Changing sizes | 4.21 |  |  |  |
| Atlas upload per frame | Many glyphs (steady state) | 0 B |  |  |  |

## Backends

CPU time of one call of the backend's render function. "New frame" follows a `NewFrame`/`EndFrame` pair; "redraw"
renders the same frame again. The small scene is the renderer tests' scene (85 quads in 4 draw commands), the
large one a window full of text (10,621 quads in one command).

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Render time (median) | WebGPU, small scene, new frame | 15.2 µs |  |  |  |
| Render time (median) | WebGPU, small scene, redraw | 14.6 µs |  |  |  |
| Render time (median) | WebGPU, large scene, new frame | 96.6 µs |  |  |  |
| Render time (median) | WebGPU, large scene, redraw | 95.7 µs |  |  |  |
| Render time (median) | Vulkan, small scene, new frame | 44.7 µs |  |  |  |
| Render time (median) | Vulkan, small scene, redraw | 37.6 µs |  |  |  |
| Render time (median) | Vulkan, large scene, new frame | 141 µs |  |  |  |
| Render time (median) | Vulkan, large scene, redraw | 81.8 µs |  |  |  |
| Render time (median) | OpenGL, small scene, new frame | 8.69 µs |  |  |  |
| Render time (median) | OpenGL, small scene, redraw | 10.3 µs |  |  |  |
| Render time (median) | OpenGL, large scene, new frame | 81.1 µs |  |  |  |
| Render time (median) | OpenGL, large scene, redraw | 81.2 µs |  |  |  |
| Render time (median) | OpenGLES, small scene, new frame | 6.96 µs |  |  |  |
| Render time (median) | OpenGLES, small scene, redraw | 6.33 µs |  |  |  |
| Render time (median) | OpenGLES, large scene, new frame | 84.1 µs |  |  |  |
| Render time (median) | OpenGLES, large scene, redraw | 87.0 µs |  |  |  |
| Render time (median) | DX11, small scene, new frame | 22.2 µs |  |  |  |
| Render time (median) | DX11, small scene, redraw | 22.3 µs |  |  |  |
| Render time (median) | DX11, large scene, new frame | 89.0 µs |  |  |  |
| Render time (median) | DX11, large scene, redraw | 77.9 µs |  |  |  |
| Render time (median) | DX9, small scene, new frame | 37.8 µs |  |  |  |
| Render time (median) | DX9, small scene, redraw | 37.4 µs |  |  |  |
| Render time (median) | DX9, large scene, new frame | 293 µs |  |  |  |
| Render time (median) | DX9, large scene, redraw | 270 µs |  |  |  |

## Idle

How many frames an event-driven host renders while nothing happens. The simulated host renders a frame while
`IsAnimating()` is true and otherwise sleeps until the next event. "Frame time" here is the CPU time of all
frames of the simulated period.

- **Focused text field**: a form with one focused text field and no input, for ten seconds. Only the caret blinks.
- **Static interface**: the same form without focus; the one frame is the one the host renders to begin with.
- **After scrolling**: one notch of the mouse wheel over a scroll view, then nothing for three seconds: the frames
  until the scroll indicator has faded out again.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frames per second | Focused text field | 60 |  |  |  |
| CPU time per 10 s | Focused text field | 1.08 ms |  |  |  |
| Frames per second | Static interface | 0.10 |  |  |  |
| Frames per wheel notch | After scrolling | 75 |  |  |  |
| CPU time per wheel notch | After scrolling | 3.61 ms |  |  |  |

## Charts and controls

- **Line chart**: one series of 1,000 to 100,000 values in a plot about 1,200 points wide, without and with a
  label per value.
- **PopUpButton**, closed, with 10 to 1,000 items; **SegmentedControl** with 4 and 12 segments.
- **ComboBox** with its list open, showing all of 100 to 10,000 items, or the ones that match a typed "9".

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | Line chart, 1,000 points | 35.3 µs |  |  |  |
| Frame time (median) | Line chart, 1,000 points, with labels | 81.4 µs |  |  |  |
| Frame time (median) | Line chart, 10,000 points | 333 µs |  |  |  |
| Frame time (median) | Line chart, 10,000 points, with labels | 802 µs |  |  |  |
| Frame time (median) | Line chart, 100,000 points | 3.36 ms |  |  |  |
| Frame time (median) | Line chart, 100,000 points, with labels | 29 ms |  |  |  |
| Frame time (median) | PopUpButton, 10 items | 1.62 µs |  |  |  |
| Frame time (median) | PopUpButton, 100 items | 5.15 µs |  |  |  |
| Frame time (median) | PopUpButton, 1,000 items | 47.6 µs |  |  |  |
| Frame time (median) | SegmentedControl, 4 segments | 3.14 µs |  |  |  |
| Frame time (median) | SegmentedControl, 12 segments | 8.42 µs |  |  |  |
| Frame time (median) | ComboBox open, 100 items | 16.6 µs |  |  |  |
| Frame time (median) | ComboBox open, 100 items, filtered | 10.2 µs |  |  |  |
| Frame time (median) | ComboBox open, 1,000 items | 114 µs |  |  |  |
| Frame time (median) | ComboBox open, 1,000 items, filtered | 56.6 µs |  |  |  |
| Frame time (median) | ComboBox open, 10,000 items | 1.24 ms |  |  |  |
| Frame time (median) | ComboBox open, 10,000 items, filtered | 634 µs |  |  |  |

## Full frames

Whole frames of the Gallery example at its window size: sidebar, header and one page, built by the example's own
page code. These are the frames an ordinary application has.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Frame time (median) | Gallery, Typography page | 39.3 µs |  |  |  |
| Frame time (median) | Gallery, Icons page | 29.6 µs |  |  |  |
| Frame time (median) | Gallery, Buttons page | 35.0 µs |  |  |  |
| Frame time (median) | Gallery, Toggles page | 29.9 µs |  |  |  |
| Frame time (median) | Gallery, Sliders page | 22.9 µs |  |  |  |
| Frame time (median) | Gallery, TextFields page | 27.3 µs |  |  |  |
| Frame time (median) | Gallery, Layout page | 57.2 µs |  |  |  |
| Frame time (median) | Gallery, Selection page | 83.7 µs |  |  |  |
| Frame time (median) | Gallery, Dates page | 107 µs |  |  |  |
| Frame time (median) | Gallery, Menus page | 52.7 µs |  |  |  |
| Frame time (median) | Gallery, Toolbars page | 62.9 µs |  |  |  |
| Frame time (median) | Gallery, Lists page | 52.2 µs |  |  |  |
| Frame time (median) | Gallery, Hierarchies page | 68.7 µs |  |  |  |
| Frame time (median) | Gallery, Navigation page | 40.4 µs |  |  |  |
| Frame time (median) | Gallery, Charts page | 34.4 µs |  |  |  |
| Frame time (median) | Gallery, Reflection page | 70.7 µs |  |  |  |

## Build

All targets (the three libraries, every example once per backend, the tests and the benchmarks) with all six
renderer backends, built with Ninja on 16 threads.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Clean build (Debug) | all targets | 86.3 s | --- |  |  |
| Clean build, CPU seconds (Debug) | all targets | 1345 s | --- |  |  |
| Configure (Debug) | empty build directory | 8.3 s | --- |  |  |
| Rebuild (Debug) | after touching `Core/Log.h` | 49.2 s | --- |  |  |
| Rebuild, CPU seconds (Debug) | after touching `Core/Log.h` | 823 s | --- |  |  |
| Rebuild (Debug) | after changing the project version | 15.6 s | --- |  |  |
| Clean build (Release) | all targets | 96.0 s | --- |  |  |
| Clean build, CPU seconds (Release) | all targets | 1679 s | --- |  |  |
| Configure (Release) | empty build directory | 8.2 s | --- |  |  |
| Rebuild (Release) | after touching `Core/Log.h` | 67.6 s | --- |  |  |
| Rebuild, CPU seconds (Release) | after touching `Core/Log.h` | 1178 s | --- |  |  |
| Rebuild (Release) | after changing the project version | 17.6 s | --- |  |  |

## Tests

`ctest` on the Release build, as CI runs it. The backends' tests are the `Backends/*` entries: the renderer
tests, the smoke tests and the comparison with WebGPU, once per backend. On this machine every backend has a
device, so none of them is skipped.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| `ctest` wall time | whole suite | 31.2 s | --- |  |  |
| `ctest` wall time | `Backends/*` tests | 20.7 s | --- |  |  |
| `ctest` entries | whole suite | 703 | --- |  |  |

## CI

Duration of each job of a green run of `.github/workflows/CI.yml` on `main`, from GitHub's API.

| Metric | Case | v1 | v2 | v3 | Change |
| --- | --- | --- | --- | --- | --- |
| Job duration | clang-format | 7 s | --- |  |  |
| Job duration | Dawn (Windows) | 5 s | --- |  |  |
| Job duration | Dawn (Linux) | 4 s | --- |  |  |
| Job duration | Emscripten (WebGL 2) | 398 s | --- |  |  |
| Job duration | Windows MSVC (Release) | 686 s | --- |  |  |
| Job duration | Linux GCC (Debug) | 258 s | --- |  |  |
| Job duration | Linux Clang (Release) | 362 s | --- |  |  |
| Run duration | all jobs, wall clock | 697 s | --- |  |  |

## Changes

### v1

The baseline. What it shows:

- **Rows are the largest cost.** A frame is linear in the number of rows, about 1.1 µs per row for a table:
  0.56 ms at 1,000 rows and 111 ms at 100,000, wherever the view is scrolled to. Every row is laid out,
  hit-tested, has its state looked up and its text shaped, visible or not, although the draw data is that of
  the 30 visible rows either way.
- **Stacks in a loop are quadratic.** 100 stacks take 0.1 ms, 1,000 take 10 ms and 10,000 take 1.2 s: every
  stack probes the IDs of all the stacks before it from the same call site.
- **Text out of view is not free.** 10,000 labels of which 30 are visible take 2.2 ms. Changing strings allocate
  7 times per string and frame.
- **Charts draw what cannot be seen.** A line chart draws one quad per value: 100,000 points are 18 MB of draw
  data for a plot 1,200 points wide, and a label per value costs another 26 ms for measuring all of them.
- **Backends upload a frame every time it is rendered.** Rendering a frame again costs as much as rendering a
  new one everywhere but on Vulkan.
- **A focused text field keeps the host awake**: 60 frames per second for a caret that changes twice a second,
  and 75 frames follow one notch of the mouse wheel.
- **Ordinary frames are fast.** The Gallery's pages take 23 to 107 µs.

### v2: runtime

None yet.

### v3: build and CI

None yet.

### Tried and rejected

None yet.

### Not significant

Findings of the survey that the baseline did not confirm as a cost worth changing.

None yet.
