#include <cstdint>
#include <iostream>
#include <limits>
#include <new>
#include <thread>

#include "profile_allocations.h"

int main()
{
    using RelayCore::test::allocation_sample;
    const auto before = allocation_sample();
    auto* ordinary = ::operator new(37);
    auto* aligned = ::operator new[](91, std::align_val_t{256});
    auto* empty = ::operator new(0);
    const auto during = allocation_sample();
    if (during.count - before.count != 3 || during.bytes - before.bytes != 128
        || reinterpret_cast<uintptr_t>(aligned) % 256 || !empty)
        return 1;
    ::operator delete(ordinary, size_t{37});
    ::operator delete[](aligned, std::align_val_t{256});
    ::operator delete(empty);
    ::operator delete(nullptr);
    if (::operator new(std::numeric_limits<size_t>::max(), std::nothrow) != nullptr)
        return 1;
    // Keep the CRT pointer layout for ownership transfers across threads and DLLs.
    auto* transferred = ::operator new(113);
    {
        std::thread consumer{[transferred] {
            ::operator delete(transferred);
        }};
        consumer.join();
    }
    std::cout << "C++ allocation counts, alignment, overflow and cross-thread free passed\n";
}
