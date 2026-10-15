#pragma once

#include <cstdint>

namespace RelayCore::test
{
    struct AllocationSample {
        uint64_t count = 0, bytes = 0;
    };

    // Executable-local C++ new calls only; excludes C heaps and allocations inside DLLs.
    // Free/retained totals cannot be inferred across DLL ownership transfers.
    [[nodiscard]] auto allocation_sample() noexcept -> AllocationSample;
} // namespace RelayCore::test
