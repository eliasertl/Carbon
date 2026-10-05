# Carbon — working rules

Carbon is an immediate-mode C++20 UI framework with a macOS look, rendered through WebGPU (Dawn). The design
contract is [Docs/Architecture.md](Docs/Architecture.md); read it before changing public API.

## Code conventions

- Everything lives in `namespace Carbon`. Implementation helpers go in `Carbon::Internal`. Nothing goes into the
  global namespace except macros, which are prefixed `CB_`. CMake variables and options are prefixed `CARBON_`.
- Naming:
  - Types, functions, methods, namespaces, enum values: `PascalCase`. Enums are `enum class`.
  - Class members: `m_PascalCase`. Globals: `g_PascalCase`. Static members and file-level statics: `s_PascalCase`.
  - Public data fields of plain structs (option structs, math types): `PascalCase` (`options.CornerRadius`).
  - Locals and parameters: `camelCase`.
  - Constants (`constexpr`, `const` at namespace or class scope): `PascalCase` (`DefaultSmoothing`).
  - Template parameters: `T` or `PascalCase`.
  - Per-call option structs are named `<Widget>Options`. Every field of a struct that is meant for designated
    initializers has a default member initializer (`= {}` if nothing else): GCC's `-Wextra` otherwise warns about
    each omitted field in the caller's code.
- Class layout: methods and members sit in separate sections with repeated access specifiers. Public methods come
  first, then private methods, then private members.
- Avoid `auto`. Use it only when the type is long and obvious from the same line (iterators, lambdas).
- Files are `PascalCase` and match their main type (`GlyphAtlas.h`, `GlyphAtlas.cpp`), one main type per
  header/source pair. Use `#pragma once`. Include from the `src` root: `#include "Carbon/Core/Log.h"`.
- All folder names are `PascalCase`, except `src`.
- Every public function and type gets a short `///` comment.
- Internal headers end in `Internal.h` or live in an `Internal/` folder, and are not listed as public headers in
  CMake. `Extensions/` and `Examples/CustomComponent` may include public Carbon headers only.
- Only `Framework/src/Carbon/Renderer/` calls Dawn. Everything else must stay GPU-free and unit-testable.
- No per-frame heap allocations in steady state: reuse buffers, cache shaped text, take `std::string_view`.
- Format messages with `std::format`. No logging library; logs and asserts go through the host's callbacks.

## Build

- CMake 3.25+, C++20. Windows: MSVC 2022. Linux: GCC 13+ or Clang 17+.
- Dawn is never built by Carbon. Pass its install prefix: `-DCMAKE_PREFIX_PATH=<dawn-install>`. With MSVC the
  Dawn configuration must match (Debug Dawn for Debug Carbon); see [Docs/Building.md](Docs/Building.md).
- The root `CMakeLists.txt` holds only options and `add_subdirectory` calls. Never use `CMAKE_SOURCE_DIR`; never
  set global flags or output directories. Warning flags apply to Carbon targets only (`carbon_configure_target`).
- Embedded assets (fonts, shaders, generated icon constants) are generated into the build tree. Never commit them.

## Workflow

- Work milestone by milestone (see Architecture.md, section 15). At the end of each milestone: build Debug and
  Release with zero warnings in Carbon targets, run all tests, run clang-format, update the docs, commit, push.
- Run `Scripts/Format.ps1` (or `Scripts/Format.sh`) before every commit. `.clang-format` is fixed; do not edit it.
- Commit in small, logical steps with clear messages.
- Never commit build outputs, generated files, IDE folders, logs, self-check screenshots or scratch code.
  Throwaway files go in the gitignored `Scratch/` folder.
- When a new file establishes a better name or pattern, rename older files with `git mv` and update includes,
  CMake and docs in the same commit.
- Documentation images are rendered from `Docs/Images/Screenshots.txt` by `Scripts/Screenshots.py` (CI commits
  them after pushes to `main`). Never edit or hand-crop an image; change the list. Every component page shows one.
- Check visuals yourself: every example accepts `--screenshot <file.png>`, `--theme light|dark` and
  `--scale <factor>`. Look at the result and compare it with the HIG (macOS flavor) before calling it done.
- Documentation is part of every milestone. New components get `Docs/Components/<Name>.md`.
- Decisions that deviate from or refine the plan are recorded in the decision log of Architecture.md.
