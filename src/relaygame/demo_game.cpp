/**
 * @file src/relaygame/demo_game.cpp
 * @brief Demo game implementation.
 */

#include "relaygame/demo_game.h"

#include <type_traits>
#include <utility>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_loader.h"

namespace
{
    /**
     * @brief Helper constant used to trigger a dependent static_assert in templates.
     * @tparam Type Type participating in the dependent false value.
     */
    template<typename>
    inline constexpr bool always_false_v = false;

    /**
     * @brief Passes through an existing glm::vec3 value.
     * @param v Source vector.
     * @return The same vector.
     */
    inline auto to_vec3(const glm::vec3& v) -> glm::vec3 { return v; }

    /**
     * @brief Converts any x/y/z-like vector object to glm::vec3.
     * @tparam T Source vector-like type.
     * @param v Source value exposing x, y, and z members.
     * @return Converted glm::vec3 value.
     */
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

    /**
     * @brief Detects blockout solids that expose min and max members.
     * @tparam T Candidate solid type.
     */
    template<typename T>
    concept HasMinMax = requires(const T& s)
    {
        s.min;
        s.max;
    };

    /**
     * @brief Detects blockout solids that expose mins and maxs members.
     * @tparam T Candidate solid type.
     */
    template<typename T>
    concept HasMinsMaxs = requires(const T& s)
    {
        s.mins;
        s.maxs;
    };

    /**
     * @brief Detects solids described by center and size members.
     * @tparam T Candidate solid type.
     */
    template<typename T>
    concept HasCenterSize = requires(const T& s)
    {
        s.center;
        s.size;
    };

    /**
     * @brief Detects solids described by position and size members.
     * @tparam T Candidate solid type.
     */
    template<typename T>
    concept HasPositionSize = requires(const T& s)
    {
        s.position;
        s.size;
    };

    /**
     * @brief Detects solids described by origin and extents members.
     * @tparam T Candidate solid type.
     */
    template<typename T>
    concept HasOriginExtents = requires(const T& s)
    {
        s.origin;
        s.extents;
    };

    /**
     * @brief Extracts a common center/size representation from several solid layouts.
     * @tparam SolidT Solid type to adapt.
     * @param solid Solid instance to inspect.
     * @return Pair containing the solid center and size vectors.
     */
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
