/**
 * @file src/relaycore/platform/sdl_system.h
 * @brief SDL lifetime wrapper for the engine runtime.
 */

#pragma once

namespace RelayCore
{
    /**
     * @brief Initializes SDL on construction and shuts it down on destruction.
     */
    class SdlSystem {
    public:
        /**
         * @brief Initializes the SDL subsystems required by the engine.
         */
        SdlSystem();

        /**
         * @brief Shuts down SDL.
         */
        ~SdlSystem() noexcept;

        SdlSystem(const SdlSystem&) = delete; ///< SdlSystem is not copyable.

        /**
         * @brief Disables copy assignment.
         * @return Reference to this object.
         */
        auto operator=(const SdlSystem&) -> SdlSystem& = delete;

        SdlSystem(SdlSystem&&) = delete; ///< SdlSystem is not movable.

        /**
         * @brief Disables move assignment.
         * @return Reference to this object.
         */
        auto operator=(SdlSystem&&) -> SdlSystem& = delete;
    };
} // namespace RelayCore
