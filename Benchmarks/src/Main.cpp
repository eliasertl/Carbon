// CarbonBenchmarks: Carbon's benchmarks, see Docs/Optimizations.md.
//
//   CarbonBenchmarks [--benchmark_filter=<regex>] [--benchmark_repetitions=<n>] [--benchmark_out=<file.json>] ...
//
// Every option of Google Benchmark works. Scripts/Benchmarks.py runs the whole set and prints the table rows of
// Docs/Optimizations.md.

#include <benchmark/benchmark.h>

#include <Carbon/Carbon.h>

int main(int argc, char** argv)
{
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
        return 1;
    benchmark::AddCustomContext("carbon_version", std::string(Carbon::GetVersionString()));
#if defined(NDEBUG)
    benchmark::AddCustomContext("carbon_build_type", "optimized");
#else
    benchmark::AddCustomContext("carbon_build_type", "debug (do not use these numbers)");
#endif
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
