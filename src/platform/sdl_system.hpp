#pragma once

namespace RelayCore
{
    class SdlSystem {
    public:
        SdlSystem();

        ~SdlSystem() noexcept;

        SdlSystem(const SdlSystem&) = delete;

        auto operator=(const SdlSystem&) -> SdlSystem& = delete;

        SdlSystem(SdlSystem&&) = delete;

        auto operator=(SdlSystem&&) -> SdlSystem& = delete;
    };
} // namespace RelayCore
