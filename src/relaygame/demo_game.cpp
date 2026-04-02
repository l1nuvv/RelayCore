#include "relaygame/demo_game.hpp"

#include <type_traits>
#include <utility>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "relaycore/app/engine_services.hpp"
#include "relaycore/core/log.hpp"
#include "relaycore/core/paths.hpp"
#include "relaycore/render/renderer.hpp"
#include "relaycore/world/blockout_loader.hpp"

namespace
{
    template<typename>
    inline constexpr bool always_false_v = false;

    inline auto to_vec3(const glm::vec3& v) -> glm::vec3 { return v; }

    template<typename T>
        requires requires(const T& v)
        {
            v.x;
            v.y;
            v.z;
        }
    auto to_vec3(const T& v) -> glm::vec3
    {
        return glm::vec3(
            static_cast<float>(v.x),
            static_cast<float>(v.y),
            static_cast<float>(v.z));
    }

    template<typename T>
    concept HasMinMax = requires(const T& s)
    {
        s.min;
        s.max;
    };

    template<typename T>
    concept HasMinsMaxs = requires(const T& s)
    {
        s.mins;
        s.maxs;
    };

    template<typename T>
    concept HasCenterSize = requires(const T& s)
    {
        s.center;
        s.size;
    };

    template<typename T>
    concept HasPositionSize = requires(const T& s)
    {
        s.position;
        s.size;
    };

    template<typename T>
    concept HasOriginExtents = requires(const T& s)
    {
        s.origin;
        s.extents;
    };

    template<typename SolidT>
    auto solid_center_and_size(const SolidT& solid) -> std::pair<glm::vec3, glm::vec3>
    {
        if constexpr (HasMinMax<SolidT>)
        {
            const glm::vec3 min = to_vec3(solid.min);
            const glm::vec3 max = to_vec3(solid.max);
            return {(min + max) * 0.5F, max - min};
        } else if constexpr (HasMinsMaxs<SolidT>)
        {
            const glm::vec3 min = to_vec3(solid.mins);
            const glm::vec3 max = to_vec3(solid.maxs);
            return {(min + max) * 0.5F, max - min};
        } else if constexpr (HasCenterSize<SolidT>) { return {to_vec3(solid.center), to_vec3(solid.size)}; } else if
        constexpr (HasPositionSize<SolidT>) { return {to_vec3(solid.position), to_vec3(solid.size)}; } else if constexpr
        (HasOriginExtents<SolidT>)
        {
            const glm::vec3 origin  = to_vec3(solid.origin);
            const glm::vec3 extents = to_vec3(solid.extents);
            return {origin, extents * 2.0F};
        } else
        {
            static_assert(always_false_v<SolidT>,
                          "Unsupported blockout solid layout. Add an adapter in solid_center_and_size().");
        }
    }
} // namespace

namespace RelayGame
{
    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        fly_camera_.enable_capture(services.window());

        const auto level_path = RelayCore::core::asset_path("levels/test_room.toml");
        level_                = RelayCore::load_blockout_level(level_path);

        RelayCore::log::info(
            "DemoGame loaded level: {} solids, {} entities",
            level_.solids.size(),
            level_.entities.size());
    }

    void DemoGame::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        fly_camera_.on_event(event, services.window(), services);
    }

    void DemoGame::on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        fly_camera_.update(frame, services.camera());
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& /*frame*/, RelayCore::EngineServices& services)
    {
        for (const auto& solid: level_.solids)
        {
            const auto [center, size] = solid_center_and_size(solid);

            if (size.x <= 0.0F || size.y <= 0.0F || size.z <= 0.0F) { continue; }

            glm::mat4 model(1.0F);
            model = glm::translate(model, center);
            model = glm::scale(model, size);

            services.renderer().draw_color_cube(model);
        }
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& /*services*/) noexcept
    {
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate> { return std::make_unique<DemoGame>(); }
} // namespace RelayGame
