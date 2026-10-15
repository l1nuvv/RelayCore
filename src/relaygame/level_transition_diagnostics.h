#pragma once

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaygame/game_world_runtime.h"

#include <chrono>
#include <string>
#include <string_view>

namespace RelayGame::detail
{
    // Success marks runtime activation, independently of later progress writes or UI publication.
    class LevelTransitionDiagnostics {
    public:
        LevelTransitionDiagnostics(
            RelayCore::EngineServices& services,
            const GameWorldRuntime& runtime,
            std::string_view reason,
            std::string destination,
            std::string_view entry = {}
        ) :
            runtime_(runtime), reason_(reason), source_(runtime.level_path()), destination_(std::move(destination)),
            entry_(entry)
        {
            services.frame_diagnostics().mark_loading();
            RelayCore::log::info(
                "[gameplay] Level transition begin: reason={} source='{}' destination='{}' entry='{}'",
                reason_,
                source_,
                destination_,
                entry_
            );
        }
        ~LevelTransitionDiagnostics() noexcept
        {
            if (!finished_)
                try
                {
                    failed("preparation aborted by exception; see associated error");
                } catch (...)
                {}
        }
        LevelTransitionDiagnostics(const LevelTransitionDiagnostics&) = delete;
        auto operator=(const LevelTransitionDiagnostics&) -> LevelTransitionDiagnostics& = delete;
        void succeeded()
        {
            finish(true, {});
        }
        void failed(std::string_view error)
        {
            finish(false, error);
        }

    private:
        void finish(bool success, std::string_view error)
        {
            finished_ = true;
            const auto duration = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
            const auto message = fmt::format(
                "[gameplay] Level transition {}: reason={} source='{}' destination='{}' entry='{}' duration_ms={:.3f} "
                "active='{}' "
                "error='{}'",
                success ? "success" : "failed",
                reason_,
                source_,
                destination_,
                entry_,
                duration,
                runtime_.level_path(),
                error
            );
            if (success)
                RelayCore::log::info(message);
            else
                RelayCore::log::error(message);
        }
        using Clock = std::chrono::steady_clock;
        const GameWorldRuntime& runtime_;
        std::string reason_, source_, destination_, entry_;
        Clock::time_point start_ = Clock::now();
        bool finished_ = false;
    };
} // namespace RelayGame::detail
