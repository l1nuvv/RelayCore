#include "relaygame/save_game.h"

#include "relaycore/world/blockout_level.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <toml++/toml.hpp>
#include <unordered_set>

namespace RelayGame
{
    namespace
    {
        template<typename T>
        auto required(const toml::table& table, const char* key) -> T
        {
            const auto value = table[key].value<T>();
            if (!value)
                throw std::runtime_error(std::string{"Save: missing or invalid field "} + key);
            return *value;
        }

        auto number(const toml::table& table, const char* key, float low = 0, float high = 1.0e9F) -> float
        {
            const auto value = required<double>(table, key);
            if (!std::isfinite(value) || value < low || value > high)
                throw std::runtime_error(std::string{"Save: out-of-range field "} + key);
            return static_cast<float>(value);
        }

        auto identifier(const toml::table& table, const char* key) -> std::uint64_t
        {
            const auto text = required<std::string>(table, key);
            std::uint64_t result = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
                throw std::runtime_error(std::string{"Save: invalid identifier "} + key);
            return result;
        }

        auto count(const toml::table& table, const char* key) -> std::uint32_t
        {
            const auto value = required<std::int64_t>(table, key);
            if (value < 0 || value > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error(std::string{"Save: invalid counter "} + key);
            return static_cast<std::uint32_t>(value);
        }

        auto array(const toml::table& table, const char* key) -> const toml::array&
        {
            const auto* value = table[key].as_array();
            if (!value || value->size() > 10000)
                throw std::runtime_error(std::string{"Save: invalid array "} + key);
            return *value;
        }

        auto table_at(const toml::node& node) -> const toml::table&
        {
            const auto* value = node.as_table();
            if (!value)
                throw std::runtime_error("Save: expected a table");
            return *value;
        }

        auto vector(const toml::table& table, const char* key) -> glm::vec3
        {
            const auto& values = array(table, key);
            if (values.size() != 3)
                throw std::runtime_error(std::string{"Save: expected three coordinates for "} + key);
            glm::vec3 result;
            for (int i = 0; i < 3; ++i)
            {
                const auto value = values[i].value<double>();
                if (!value || !std::isfinite(*value) || std::abs(*value) > 1.0e6)
                    throw std::runtime_error(std::string{"Save: invalid coordinate in "} + key);
                result[i] = static_cast<float>(*value);
            }
            return result;
        }

        auto strings(const toml::table& table, const char* key) -> std::vector<std::string>
        {
            std::vector<std::string> result;
            for (const auto& node : array(table, key))
            {
                const auto value = node.value<std::string>();
                if (!value || value->empty() || std::ranges::find(result, *value) != result.end())
                    throw std::runtime_error(std::string{"Save: invalid or duplicate entry in "} + key);
                result.push_back(*value);
            }
            return result;
        }

        auto pack_strings(const std::vector<std::string>& values) -> toml::array
        {
            toml::array result;
            for (const auto& value : values)
                result.push_back(value);
            return result;
        }

        auto decode(const toml::table& table, bool visited = false) -> SavedGame
        {
            const auto version = required<std::int64_t>(table, "format_version");
            if (version != 1 && version != 2 && version != 3 && version != saved_game_format_version)
                throw std::runtime_error(
                    "Save: unsupported format version " + std::to_string(version) + "; expected "
                    + std::to_string(saved_game_format_version)
                );
            SavedGame save;
            save.level_fingerprint = identifier(table, "level_fingerprint");
            save.played_seconds = number(table, "played_seconds");
            save.flashlight = required<bool>(table, "flashlight");
            save.player.position = vector(table, "position");
            save.player.velocity = vector(table, "velocity");
            save.player.view_angles = {
                number(table, "yaw", -1.0e6F, 1.0e6F),
                number(table, "pitch", -1.55334306F, 1.55334306F)
            };
            save.player.grounded = required<bool>(table, "grounded");
            auto& world = save.world;
            world.level_path = required<std::string>(table, "level_path");
            world.registry_signature = identifier(table, "registry_signature");
            world.simulation_tick = identifier(table, "simulation_tick");
            world.inventory_item_ids = strings(table, "inventory");
            world.activated_target_ids = strings(table, "activated");
            world.signaled_target_ids = strings(table, "signaled");
            world.mission = {
                required<bool>(table, "mission_active"),
                required<bool>(table, "completed"),
                count(table, "mission_step")
            };
            std::unordered_set<std::uint64_t> ids;
            for (const auto& node : array(table, "doors"))
            {
                const auto& entry = table_at(node);
                const auto id = identifier(entry, "id");
                const auto state = count(entry, "state");
                if (!ids.insert(id).second || state > 3)
                    throw std::runtime_error("Save: duplicate door or invalid door state");
                world.doors.push_back(
                    {id, number(entry, "fraction", 0, 1), static_cast<RelayCore::gameplay::DoorState>(state)}
                );
            }
            for (const auto& node : array(table, "triggers"))
            {
                const auto& entry = table_at(node);
                const auto id = identifier(entry, "id");
                if (!ids.insert(id).second)
                    throw std::runtime_error("Save: duplicate trigger");
                world.triggers.push_back({id, required<bool>(entry, "fired")});
            }
            for (const auto& node : array(table, "usables"))
            {
                const auto& entry = table_at(node);
                const auto id = identifier(entry, "id");
                if (!ids.insert(id).second)
                    throw std::runtime_error("Save: duplicate usable");
                world.usables.push_back({id, required<bool>(entry, "locked"), required<bool>(entry, "consumed")});
            }
            for (const auto& node : array(table, "turrets"))
            {
                const auto& entry = table_at(node);
                const auto index = count(entry, "entity");
                if (index > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
                    throw std::runtime_error("Save: turret entity index is out of range");
                world.turrets.push_back(
                    {static_cast<int>(index),
                     number(entry, "health"),
                     number(entry, "cooldown"),
                     number(entry, "warmup"),
                     required<bool>(entry, "enabled"),
                     required<bool>(entry, "destroyed"),
                     required<bool>(entry, "warming_up")}
                );
            }
            const auto* combat = table["combat"].as_table();
            if (!combat)
                throw std::runtime_error("Save: missing combat state");
            world.player_combat = {
                number(*combat, "health", 0, 100),
                number(*combat, "cooldown"),
                number(*combat, "hurt_flash"),
                number(*combat, "respawn"),
                required<bool>(*combat, "alive"),
                count(*combat, "shots_fired"),
                count(*combat, "shots_hit"),
                count(*combat, "kills"),
                count(*combat, "deaths"),
                number(*combat, "damage_dealt"),
                number(*combat, "damage_taken"),
                required<std::string>(*combat, "last_damage_source")
            };
            if (world.level_path.empty() || world.registry_signature == 0 || save.level_fingerprint == 0
                || world.player_combat.alive != (world.player_combat.health > 0))
                throw std::runtime_error("Save: inconsistent player or level state");
            if (version >= 4)
            {
                const auto weapon = required<std::string>(table, "selected_weapon");
                if (weapon == "precise")
                    world.selected_weapon = PlayerWeapon::Precise;
                else if (weapon == "shotgun"
                         && std::ranges::find(world.inventory_item_ids, shotgun_item_id)
                                != world.inventory_item_ids.end())
                    world.selected_weapon = PlayerWeapon::Shotgun;
                else
                    throw std::runtime_error("Save: invalid or unavailable selected_weapon");
            }
            if (version >= 3)
            {
                std::unordered_set<int> ids;
                for (const auto& node : array(table, "enemies"))
                {
                    const auto& data = table_at(node);
                    EnemyState enemy;
                    const auto id = count(data, "entity");
                    const auto mode = count(data, "mode");
                    if (id > 100000 || mode > static_cast<unsigned>(EnemyMode::Dead)
                        || !ids.insert(static_cast<int>(id)).second)
                        throw std::runtime_error("Save: invalid enemy identity or mode");
                    enemy.entity_index = static_cast<int>(id);
                    enemy.mode = static_cast<EnemyMode>(mode);
                    enemy.position = vector(data, "position");
                    enemy.last_seen = vector(data, "last_seen");
                    enemy.yaw = number(data, "yaw", -1.0e6F, 1.0e6F);
                    enemy.health = number(data, "health", 0, 1000);
                    enemy.timer = number(data, "timer", 0, 1);
                    enemy.vertical_speed = number(data, "vertical_speed", -20, 20);
                    if ((enemy.mode == EnemyMode::Dead) != (enemy.health == 0))
                        throw std::runtime_error("Save: inconsistent enemy health");
                    save.enemies.push_back(enemy);
                }
            }
            if (version >= 2)
            {
                const auto& levels = array(table, "visited_levels");
                if (levels.size() > 64 || (visited && !levels.empty()))
                    throw std::runtime_error("Save: invalid map history");
                std::unordered_set<std::string> paths{
                    std::filesystem::path(world.level_path).lexically_normal().generic_string()
                };
                for (const auto& node : levels)
                {
                    auto previous = decode(table_at(node), true);
                    if (!paths
                             .insert(
                                 std::filesystem::path(previous.world.level_path).lexically_normal().generic_string()
                             )
                             .second)
                        throw std::runtime_error("Save: duplicate map in history");
                    save.visited_levels.push_back(std::move(previous));
                }
            }
            return save;
        }
    } // namespace

    auto fingerprint_level(const RelayCore::BlockoutLevel& level) -> std::uint64_t
    {
        std::uint64_t hash = 14695981039346656037ULL;
        const auto bytes = [&](std::string_view value) {
            for (const unsigned char byte : value)
                hash = (hash ^ byte) * 1099511628211ULL;
            hash = (hash ^ 0xFFU) * 1099511628211ULL;
        };
        const auto scalar = [&](float value) {
            bytes(std::to_string(std::bit_cast<std::uint32_t>(value)));
        };
        const auto vec = [&](glm::vec3 value) {
            scalar(value.x);
            scalar(value.y);
            scalar(value.z);
        };
        bytes(std::to_string(level.solids.size()));
        for (const auto& solid : level.solids)
        {
            vec(solid.mins);
            vec(solid.maxs);
            vec(solid.tint);
            scalar(solid.brightness);
            bytes(std::to_string(static_cast<int>(solid.material)));
        }
        bytes(std::to_string(level.entities.size()));
        for (const auto& entity : level.entities)
        {
            bytes(entity.classname);
            vec(entity.origin);
            vec(entity.angles);
            bytes(entity.has_angles ? "1" : "0");
            std::vector<std::pair<std::string, std::string>> properties(
                entity.properties.begin(),
                entity.properties.end()
            );
            std::ranges::sort(properties);
            for (const auto& [key, value] : properties)
            {
                bytes(key);
                bytes(value);
            }
        }
        return hash;
    }

    auto load_saved_game(const std::filesystem::path& path) -> SavedGame
    {
        if (std::filesystem::file_size(path) > 1024 * 1024)
            throw std::runtime_error("Save exceeds the 1 MiB size limit");
        return decode(toml::parse_file(path.string()));
    }

    static auto encode(const SavedGame& save, bool visited = false) -> toml::table
    {
        if (save.visited_levels.size() > 64 || (visited && !save.visited_levels.empty()))
            throw std::runtime_error("Save: invalid map history");
        const auto& world = save.world;
        const auto& player = save.player;
        const auto& combat = world.player_combat;
        if (world.selected_weapon != PlayerWeapon::Precise && world.selected_weapon != PlayerWeapon::Shotgun)
            throw std::runtime_error("Save: invalid selected_weapon");
        toml::table table{
            {"format_version", saved_game_format_version},
            {"selected_weapon", world.selected_weapon == PlayerWeapon::Shotgun ? "shotgun" : "precise"},
            {"level_path", world.level_path},
            {"level_fingerprint", std::to_string(save.level_fingerprint)},
            {"registry_signature", std::to_string(world.registry_signature)},
            {"simulation_tick", std::to_string(world.simulation_tick)},
            {"played_seconds", save.played_seconds},
            {"flashlight", save.flashlight},
            {"position", toml::array{player.position.x, player.position.y, player.position.z}},
            {"velocity", toml::array{player.velocity.x, player.velocity.y, player.velocity.z}},
            {"yaw", player.view_angles.x},
            {"pitch", player.view_angles.y},
            {"grounded", player.grounded},
            {"inventory", pack_strings(world.inventory_item_ids)},
            {"activated", pack_strings(world.activated_target_ids)},
            {"signaled", pack_strings(world.signaled_target_ids)},
            {"mission_active", world.mission.active},
            {"completed", world.mission.completed},
            {"mission_step", world.mission.current_step},
            {"combat",
             toml::table{
                 {"health", combat.health},
                 {"cooldown", combat.weapon_cooldown_remaining_seconds},
                 {"hurt_flash", combat.hurt_flash_remaining_seconds},
                 {"respawn", combat.respawn_remaining_seconds},
                 {"alive", combat.alive},
                 {"shots_fired", combat.shots_fired},
                 {"shots_hit", combat.shots_hit},
                 {"kills", combat.kills},
                 {"deaths", combat.deaths},
                 {"damage_dealt", combat.damage_dealt},
                 {"damage_taken", combat.damage_taken},
                 {"last_damage_source", combat.last_damage_source}
             }}
        };
        toml::array enemy_data;
        for (const auto& enemy : save.enemies)
            enemy_data.push_back(toml::table{
                {"entity", enemy.entity_index},
                {"mode", static_cast<int>(enemy.mode)},
                {"position", toml::array{enemy.position.x, enemy.position.y, enemy.position.z}},
                {"last_seen", toml::array{enemy.last_seen.x, enemy.last_seen.y, enemy.last_seen.z}},
                {"yaw", enemy.yaw},
                {"health", enemy.health},
                {"timer", enemy.timer},
                {"vertical_speed", enemy.vertical_speed}
            });
        table.insert("enemies", std::move(enemy_data));
        toml::array doors, triggers, usables, turrets;
        for (const auto& door : world.doors)
            doors.push_back(toml::table{
                {"id", std::to_string(door.runtime_id)},
                {"fraction", door.open_fraction},
                {"state", static_cast<int>(door.state)}
            });
        for (const auto& trigger : world.triggers)
            triggers.push_back(toml::table{{"id", std::to_string(trigger.runtime_id)}, {"fired", trigger.fired}});
        for (const auto& usable : world.usables)
            usables.push_back(toml::table{
                {"id", std::to_string(usable.runtime_id)},
                {"locked", usable.locked},
                {"consumed", usable.consumed}
            });
        for (const auto& turret : world.turrets)
            turrets.push_back(toml::table{
                {"entity", turret.authored_entity_index},
                {"health", turret.health},
                {"cooldown", turret.cooldown_remaining_seconds},
                {"warmup", turret.warmup_remaining_seconds},
                {"enabled", turret.enabled},
                {"destroyed", turret.destroyed},
                {"warming_up", turret.warming_up}
            });
        table.insert("doors", std::move(doors));
        table.insert("triggers", std::move(triggers));
        table.insert("usables", std::move(usables));
        table.insert("turrets", std::move(turrets));
        toml::array levels;
        for (const auto& previous : save.visited_levels)
            levels.push_back(encode(previous, true));
        table.insert("visited_levels", std::move(levels));
        return table;
    }

    void write_saved_game(const std::filesystem::path& path, const SavedGame& save)
    {
        const auto table = encode(save);
        static_cast<void>(decode(table));
        std::ostringstream serialized;
        serialized << table << '\n';
        if (serialized.str().size() > 1024 * 1024)
            throw std::runtime_error("Save exceeds the 1 MiB size limit");
        std::filesystem::create_directories(path.parent_path());
        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
            output.exceptions(std::ios::badbit | std::ios::failbit);
            output << serialized.str();
            output.close();
        }
        std::filesystem::rename(temporary, path);
    }
} // namespace RelayGame
