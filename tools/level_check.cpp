#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaygame/level_session_builder.h"

#include <cstdint>
#include <iostream>
#include <toml++/toml.hpp>

namespace
{
    auto vector(const glm::vec3& value) -> toml::array
    {
        return toml::array{value.x, value.y, value.z};
    }

    auto report(const RelayGame::LevelSession& session) -> toml::table
    {
        toml::table result{
            {"valid", true},
            {"map", session.level_path.generic_string()},
            {"spawn", vector(session.spawn_position)},
            {"counts",
             toml::table{
                 {"solids", static_cast<std::int64_t>(session.level.solids.size())},
                 {"prop_colliders", static_cast<std::int64_t>(session.level.prop_colliders.size())},
                 {"entities", static_cast<std::int64_t>(session.level.entities.size())},
                 {"doors", static_cast<std::int64_t>(session.doors.size())},
                 {"usables", static_cast<std::int64_t>(session.usables.size())},
                 {"triggers", static_cast<std::int64_t>(session.triggers.size())},
                 {"turrets", static_cast<std::int64_t>(session.combat.turrets.size())},
                 {"enemies", static_cast<std::int64_t>(session.enemies.size())},
                 {"hazards", static_cast<std::int64_t>(session.combat.hazards.size())},
                 {"mission_steps", static_cast<std::int64_t>(session.mission_definition.steps.size())}
             }}
        };
        if (!session.level.solids.empty())
        {
            auto mins = session.level.solids.front().mins;
            auto maxs = session.level.solids.front().maxs;
            for (const auto& solid : session.level.solids)
            {
                mins = glm::min(mins, solid.mins);
                maxs = glm::max(maxs, solid.maxs);
            }
            result.insert("bounds", toml::table{{"mins", vector(mins)}, {"maxs", vector(maxs)}});
        }
        toml::array entities;
        for (std::size_t i = 0; i < session.level.entities.size(); ++i)
        {
            const auto& entity = session.level.entities[i];
            toml::table properties;
            for (const auto& [key, value] : entity.properties)
                properties.insert(key, value);
            toml::table item{
                {"index", static_cast<std::int64_t>(i)},
                {"classname", entity.classname},
                {"origin", vector(entity.origin)},
                {"properties", std::move(properties)}
            };
            if (i < session.level.entity_sources.size())
            {
                const auto& source = session.level.entity_sources[i];
                item.insert(
                    "source",
                    toml::table{
                        {"file", source.file_path},
                        {"line", source.line},
                        {"column", source.column},
                        {"instance", source.instance_name}
                    }
                );
            }
            entities.push_back(std::move(item));
        }
        result.insert("entities", std::move(entities));
        toml::array bindings;
        for (const auto& binding : session.runtime_registry.bindings())
            bindings.push_back(toml::table{
                {"source", binding.source_handle},
                {"target", binding.target},
                {"resolved_count", binding.resolved_target_count},
                {"resolved_targets", binding.resolved_targets_summary},
                {"file", binding.source_file_path},
                {"line", binding.source_line}
            });
        result.insert("bindings", std::move(bindings));
        toml::array steps;
        for (const auto& step : session.mission_definition.steps)
            steps.push_back(toml::table{{"title", step.title}, {"detail", step.detail}, {"target", step.target}});
        result.insert("mission", std::move(steps));
        toml::array warnings;
        for (const auto& warning : session.authoring_warnings)
            warnings.push_back(toml::table{
                {"message", warning.message},
                {"file", warning.primary_file_path},
                {"line", warning.primary_line},
                {"object", warning.primary_label},
                {"other_file", warning.secondary_file_path},
                {"other_line", warning.secondary_line}
            });
        result.insert("warnings", std::move(warnings));
        return result;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2 && (argc != 4 || std::string_view{argv[2]} != "--entry"))
    {
        std::cerr << "Usage: relaycore_level_check MAP.toml [--entry NAME]\n";
        return 2;
    }
    int exit_code = 0;
    toml::table output;
    try
    {
        RelayCore::log::init(
            {.enable_console_sink = true,
             .enable_timestamped_log = false,
             .enable_latest_log = false,
             .level = spdlog::level::off}
        );
        const RelayCore::core::GameplayConfig config{};
        const RelayGame::LevelSessionBuilder builder{config};
        const auto path = std::filesystem::absolute(argv[1]).lexically_normal();
        const auto session = argc == 4 ? builder.build_at_entry(path, argv[3]) : builder.build(path);
        for (const auto& exit : session.exits)
            (void) builder.build_at_entry(exit.destination, exit.entry);
        output = report(session);
    } catch (const std::exception& error)
    {
        output = toml::table{{"valid", false}, {"error", error.what()}};
        exit_code = 1;
    }
    RelayCore::log::shutdown();
    std::cout << toml::json_formatter{output} << '\n';
    return exit_code;
}
