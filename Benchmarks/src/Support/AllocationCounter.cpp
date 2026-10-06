#include "Support/AllocationCounter.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
    std::atomic<size_t> g_AllocationCount{0};

    void* Allocate(std::size_t size)
    {
        g_AllocationCount.fetch_add(1, std::memory_order_relaxed);
        if (void* memory = std::malloc(size == 0 ? 1 : size))
            return memory;
        throw std::bad_alloc();
    }
} // namespace

void* operator new(std::size_t size)
{
    return Allocate(size);
}

void* operator new[](std::size_t size)
{
    return Allocate(size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace Carbon::Benchmarks
{
    size_t GetAllocationCount()
    {
        return g_AllocationCount.load(std::memory_order_relaxed);
    }
} // namespace Carbon::Benchmarks
