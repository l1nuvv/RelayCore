#pragma once

#include <memory>

#include "relaycore/app/app_delegate.hpp"

namespace RelayCore
{
    [[nodiscard]] auto run_application(
        std::unique_ptr<AppDelegate> app_delegate = std::make_unique<NullAppDelegate>(),
        int                          argc = 0,
        char**                       argv = nullptr
    ) -> int;
} // namespace RelayCore
