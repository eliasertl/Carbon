#pragma once

#include <cstddef>

namespace Carbon::Benchmarks
{
    /// The number of calls of the global `operator new` since the process started. CarbonBenchmarks replaces the
    /// global allocation functions to count them: it is the only portable way to see allocations made inside the
    /// library.
    size_t GetAllocationCount();
} // namespace Carbon::Benchmarks
