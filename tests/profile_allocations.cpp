#include "profile_allocations.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#if defined(_WIN32)
#include <malloc.h>
#endif
#include <limits>
#include <new>

namespace
{
    constinit std::atomic<uint64_t> count{}, bytes{};

    auto allocate(const size_t requested, const size_t alignment) -> void*
    {
        for (;;)
        {
            void* pointer = nullptr;
            const auto size = std::max(requested, size_t{1});
            if (!alignment)
                pointer = std::malloc(size);
#if defined(_WIN32)
            else
                pointer = _aligned_malloc(size, alignment);
#else
            else if (size <= std::numeric_limits<size_t>::max() - (alignment - 1))
                pointer = std::aligned_alloc(alignment, (size + alignment - 1) & ~(alignment - 1));
#endif
            if (pointer)
            {
                count.fetch_add(1, std::memory_order_relaxed);
                bytes.fetch_add(requested, std::memory_order_relaxed);
                return pointer;
            }
            const auto handler = std::get_new_handler();
            if (!handler)
                throw std::bad_alloc{};
            handler();
        }
    }

    void release_aligned(void* pointer) noexcept
    {
#if defined(_WIN32)
        _aligned_free(pointer);
#else
        std::free(pointer);
#endif
    }
} // namespace

auto RelayCore::test::allocation_sample() noexcept -> AllocationSample
{
    return {count.load(std::memory_order_relaxed), bytes.load(std::memory_order_relaxed)};
}

auto operator new(size_t size) -> void*
{
    return allocate(size, 0);
}
auto operator new[](size_t size) -> void*
{
    return ::operator new(size);
}
auto operator new(size_t size, std::align_val_t alignment) -> void*
{
    return allocate(size, static_cast<size_t>(alignment));
}
auto operator new[](size_t size, std::align_val_t alignment) -> void*
{
    return ::operator new(size, alignment);
}
auto operator new(size_t size, const std::nothrow_t&) noexcept -> void*
{
    try
    {
        return ::operator new(size);
    } catch (...)
    {
        return nullptr;
    }
}
auto operator new[](size_t size, const std::nothrow_t& tag) noexcept -> void*
{
    return ::operator new(size, tag);
}
auto operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept -> void*
{
    try
    {
        return ::operator new(size, alignment);
    } catch (...)
    {
        return nullptr;
    }
}
auto operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t& tag) noexcept -> void*
{
    return ::operator new(size, alignment, tag);
}
void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete(void* pointer, size_t) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer, size_t) noexcept
{
    std::free(pointer);
}
void operator delete(void* pointer, std::align_val_t) noexcept
{
    release_aligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept
{
    release_aligned(pointer);
}
void operator delete(void* pointer, size_t, std::align_val_t) noexcept
{
    release_aligned(pointer);
}
void operator delete[](void* pointer, size_t, std::align_val_t) noexcept
{
    release_aligned(pointer);
}
void operator delete(void* pointer, const std::nothrow_t&) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t&) noexcept
{
    std::free(pointer);
}
void operator delete(void* pointer, std::align_val_t, const std::nothrow_t&) noexcept
{
    release_aligned(pointer);
}
void operator delete[](void* pointer, std::align_val_t, const std::nothrow_t&) noexcept
{
    release_aligned(pointer);
}
