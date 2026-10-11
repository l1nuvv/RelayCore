#pragma once

#include "relaycore/physics/collision.h"
#include "relaycore/world/blockout_level.h"

#include <filesystem>
#include <optional>
#include <unordered_map>

namespace RelayGame
{
    struct EditorSelection {
        bool entity = false;
        std::size_t index = 0;
        auto operator==(const EditorSelection&) const -> bool = default;
    };

    class LevelEditorDocument {
    public:
        void reset(RelayCore::BlockoutLevel level);
        void new_room();
        void add_enemy(glm::vec3 position);
        void add_model(glm::vec3 position, const std::string& model, const std::string& texture);
        void add_volume(glm::vec3 position, bool exit);
        void add_door_objective();
        void
        add_level_link(glm::vec3 position, bool entry, const std::string& map, const std::string& destination_entry);
        void move_mission_step(int direction);
        void set_model_collision(bool enabled);
        void set_model_collision_asset(const std::string& reference);
        auto place_on_surface(glm::vec3 origin, glm::vec3 direction, float grid) -> bool;
        [[nodiscard]] auto validation_messages(const std::filesystem::path& source_map = {}) const
            -> std::vector<std::string>;
        [[nodiscard]] auto level() const -> const RelayCore::BlockoutLevel&
        {
            return level_;
        }
        [[nodiscard]] auto selection() const -> std::optional<EditorSelection>
        {
            return selection_;
        }
        void select(std::optional<EditorSelection> selection);
        [[nodiscard]] auto bounds(EditorSelection selection) const -> RelayCore::physics::Aabb;
        [[nodiscard]] auto pick(glm::vec3 origin, glm::vec3 direction) const -> std::optional<EditorSelection>;
        [[nodiscard]] auto visible_entity(std::size_t index) const -> bool;
        void checkpoint();
        void transform(glm::vec3 center, glm::vec3 size, bool remember = true);
        void material(RelayCore::SurfaceMaterial material);
        void add_solid(glm::vec3 center, glm::vec3 size, RelayCore::SurfaceMaterial material);
        void add_light(glm::vec3 position);
        void add_effect(glm::vec3 position, const std::string& kind);
        auto attach_decal(glm::vec3 origin, glm::vec3 direction) -> bool;
        void set_angles(glm::vec3 angles);
        void add_door(glm::vec3 position);
        void add_button(glm::vec3 position);
        void set_property(const std::string& key, const std::string& value);
        void drop_to_floor();
        void place_spawn(glm::vec3 position);
        void duplicate();
        void erase();
        void undo();
        void redo();
        [[nodiscard]] auto revision() const -> std::size_t
        {
            return revision_;
        }
        [[nodiscard]] auto dirty() const -> bool
        {
            return dirty_;
        }
        void save(const std::filesystem::path& path);

    private:
        struct Snapshot {
            RelayCore::BlockoutLevel level;
            std::optional<EditorSelection> selection;
        };
        [[nodiscard]] auto unique_name(const std::string& prefix) const -> std::string;
        void changed();
        void cache_model(const std::string& path);
        std::unordered_map<std::string, RelayCore::physics::Aabb> model_bounds_;
        mutable std::unordered_map<std::size_t, RelayCore::physics::Aabb> entity_bounds_;
        RelayCore::BlockoutLevel level_;
        std::optional<EditorSelection> selection_;
        std::vector<Snapshot> undo_;
        std::vector<Snapshot> redo_;
        std::size_t revision_ = 0;
        bool dirty_ = false;
    };
} // namespace RelayGame
