/**
 * @file src/relaycore/app/run_application.h
 * @brief Convenience entry point for running the engine application.
 */

#pragma once

#include <memory>

#include "relaycore/app/app_delegate.h"

namespace RelayCore
{
    /**
     * @brief Runs the engine using the supplied game delegate.
     * @param app_delegate Delegate that receives lifecycle callbacks.
     * @param argc Process argument count.
     * @param argv Process argument vector.
     * @return Process exit code.
     */
    [[nodiscard]] auto run_application(
        std::unique_ptr<AppDelegate> app_delegate = std::make_unique<NullAppDelegate>(),
        int                          argc = 0,
        char**                       argv = nullptr
    ) -> int;
} // namespace RelayCore
