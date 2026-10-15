#include "relaycore/core/log.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/renderer.h"
#include "relaygame/demo_game.h"
#include "relaygame/level_transition_diagnostics.h"

#include <algorithm>
#include <string>

namespace RelayGame
{
    void DemoGame::transition_to(LevelExit exit, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::transition_to");
        if (screen_ != Screen::Playing || !runtime_.player_combat_state().alive)
            return;
        detail::LevelTransitionDiagnostics transition{
            services,
            runtime_,
            exit.requires_completion ? "completion_exit" : "level_exit",
            exit.destination.string(),
            exit.entry
        };
        const std::string source_level_path = runtime_.level_path();
        std::vector<SavedGame> history;
        SavedGame arrival;
        try
        {
            auto previous = runtime_.capture_saved_game(played_seconds_, flashlight_on_);
            auto level = LevelSessionBuilder{services.config().gameplay}.build_at_entry(exit.destination, exit.entry);
            auto prepared = detail::prepare_level_session(
                services.config(),
                std::move(level),
                services.renderer().authoring_config(),
                services.renderer().color_pipeline_mode(),
                services.renderer().static_models()
            );
            arrival = prepared.runtime.capture_saved_game(played_seconds_, flashlight_on_);
            const auto destination_path = std::filesystem::path(arrival.world.level_path).lexically_normal();
            const auto source_path = std::filesystem::path(previous.world.level_path).lexically_normal();
            history = visited_levels_;
            const auto stored = std::ranges::find_if(history, [&](const auto& save) {
                return std::filesystem::path(save.world.level_path).lexically_normal() == destination_path;
            });
            if (stored != history.end() || source_path == destination_path)
            {
                const auto entry_position = arrival.player;
                auto restored = source_path == destination_path ? previous : *stored;
                restored.world.level_path = arrival.world.level_path;
                prepared.runtime.restore_saved_game(restored);
                arrival = prepared.runtime.capture_saved_game(played_seconds_, flashlight_on_);
                arrival.player = entry_position;
            }
            arrival.world.inventory_item_ids = previous.world.inventory_item_ids;
            arrival.world.selected_weapon = previous.world.selected_weapon;
            arrival.world.player_combat = previous.world.player_combat;
            arrival.world.player_combat.weapon_cooldown_remaining_seconds = 0;
            arrival.world.player_combat.hurt_flash_remaining_seconds = 0;
            prepared.runtime.restore_saved_game(arrival);
            for (const auto& door : prepared.runtime.doors())
                if (RelayCore::gameplay::is_door_blocking(door)
                    && RelayCore::physics::overlaps(
                        RelayCore::physics::make_player_aabb(arrival.player.position, .3F, 1.8F),
                        RelayCore::gameplay::current_door_bounds(door),
                        .001F
                    ))
                    throw std::runtime_error("Destination entry is blocked by door: " + door.name);
            std::erase_if(history, [&](const auto& save) {
                const auto path = std::filesystem::path(save.world.level_path).lexically_normal();
                return path == destination_path || path == source_path;
            });
            if (source_path != destination_path)
                history.push_back(std::move(previous));
            arrival.visited_levels = history;
            install_level_session(std::move(prepared), services);
            transition.succeeded();
        } catch (const std::exception& error)
        {
            progress_message_.clear();
            set_pause(true, services);
            const auto save_error = std::move(progress_message_);
            progress_message_ = "Переход из '" + source_level_path + "' в '" + exit.destination.string() + "', вход '"
                                + exit.entry + "' не выполнен: " + error.what();
            if (!save_error.empty())
                progress_message_ += "\n" + save_error;
            transition.failed(error.what());
            RelayCore::log::error("{}", progress_message_);
            return;
        }

        visited_levels_ = std::move(history);
        saved_step_ = runtime_.mission_status().current_step;
        autosave_elapsed_ = 0;
        progress_message_.clear();
        if (progress_enabled_ && !editor_return_)
        {
            try
            {
                write_saved_game(progress_path_, arrival);
                disk_save_ = std::move(arrival);
                save_file_exists_ = true;
            } catch (const std::exception& error)
            {
                progress_message_ = std::string{"Уровень загружен, но прогресс не сохранён: "} + error.what();
                RelayCore::log::error("{}", progress_message_);
            }
        }
        try
        {
            runtime_.publish_diagnostics(services.renderer());
        } catch (const std::exception& error)
        {
            RelayCore::log::error("Level loaded, but diagnostics publication failed: {}", error.what());
        }
    }
} // namespace RelayGame
