#include "relaygame/demo_game.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "relaycore/app/engine_services.h"
#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"
#include "relaycore/render/blockout_mesh_builder.h"
#include "relaycore/render/renderer.h"
#include "relaycore/world/blockout_loader.h"

namespace RelayGame
{
    namespace
    {
        struct FrustumPlane {
            glm::vec3 normal{0.0F, 0.0F, 1.0F};
            float     distance = 0.0F;
        };

        struct ViewFrustum {
            std::array<FrustumPlane, 6> planes{};
        };

        [[nodiscard]] auto normalize_plane(const glm::vec4& plane) -> FrustumPlane
        {
            const glm::vec3 normal{plane.x, plane.y, plane.z};
            const float     length = glm::length(normal);
            if (length <= 0.0001F) { return {}; }

            return {normal / length, plane.w / length};
        }

        [[nodiscard]] auto extract_frustum(const glm::mat4& view_projection) -> ViewFrustum
        {
            const glm::vec4 row0{
                view_projection[0][0], view_projection[1][0], view_projection[2][0], view_projection[3][0]
            };
            const glm::vec4 row1{
                view_projection[0][1], view_projection[1][1], view_projection[2][1], view_projection[3][1]
            };
            const glm::vec4 row2{
                view_projection[0][2], view_projection[1][2], view_projection[2][2], view_projection[3][2]
            };
            const glm::vec4 row3{
                view_projection[0][3], view_projection[1][3], view_projection[2][3], view_projection[3][3]
            };

            ViewFrustum frustum{};
            frustum.planes[0] = normalize_plane(row3 + row0);
            frustum.planes[1] = normalize_plane(row3 - row0);
            frustum.planes[2] = normalize_plane(row3 + row1);
            frustum.planes[3] = normalize_plane(row3 - row1);
            frustum.planes[4] = normalize_plane(row3 + row2);
            frustum.planes[5] = normalize_plane(row3 - row2);
            return frustum;
        }

        [[nodiscard]] auto
        aabb_intersects_frustum(const ViewFrustum& frustum, const glm::vec3& mins, const glm::vec3& maxs) -> bool
        {
            for (const FrustumPlane& plane: frustum.planes)
            {
                const glm::vec3 positive_vertex{
                    plane.normal.x >= 0.0F ? maxs.x : mins.x,
                    plane.normal.y >= 0.0F ? maxs.y : mins.y,
                    plane.normal.z >= 0.0F ? maxs.z : mins.z,
                };

                if (glm::dot(plane.normal, positive_vertex) + plane.distance < 0.0F) { return false; }
            }

            return true;
        }
    } // namespace

    void DemoGame::on_startup(RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_startup");

        const auto level_path = RelayCore::core::asset_path("levels/stress_room_02.toml");
        level_                = RelayCore::load_blockout_level(level_path);

        const RelayCore::render::BlockoutRenderData render_data = RelayCore::render::build_blockout_render_data(level_);

        render_chunks_.clear();
        render_chunks_.reserve(render_data.chunks.size());

        for (const auto& chunk_data: render_data.chunks)
        {
            RenderChunk chunk{};
            chunk.mesh.initialize(chunk_data.mesh);
            chunk.mins           = chunk_data.mins;
            chunk.maxs           = chunk_data.maxs;
            chunk.triangle_count = chunk_data.triangle_count;
            render_chunks_.push_back(std::move(chunk));
        }

        total_solids_         = static_cast<std::uint32_t>(level_.solids.size());
        visible_solids_       = static_cast<std::uint32_t>(render_chunks_.size());
        draw_calls_per_frame_ = visible_solids_;
        triangles_per_frame_  = render_data.total_triangles;

        glm::vec3  spawn_position{0.0F, 0.3F, 0.0F};
        const auto spawn_it =
            std::find_if(level_.entities.begin(), level_.entities.end(), [](const RelayCore::EntitySpawnData& entity) {
                return entity.classname == "player_spawn";
            });

        if (spawn_it != level_.entities.end()) { spawn_position = spawn_it->origin; }

        player_controller_.initialize(spawn_position);
        player_controller_.enable_capture(services.window());
        player_controller_.apply_camera(services.camera());

        RelayCore::render::WorldDiagnostics world_diagnostics{};
        world_diagnostics.total_solids   = total_solids_;
        world_diagnostics.visible_solids = visible_solids_;
        world_diagnostics.draw_calls     = draw_calls_per_frame_;
        world_diagnostics.triangles      = triangles_per_frame_;
        services.renderer().set_world_diagnostics(world_diagnostics);

        RelayCore::log::info(
            "DemoGame loaded level: {} solids, {} entities, {} render chunks, {} triangles",
            level_.solids.size(),
            level_.entities.size(),
            render_chunks_.size(),
            render_data.total_triangles
        );
    }

    void DemoGame::on_event(const SDL_Event& event, RelayCore::EngineServices& services)
    {
        player_controller_.on_event(event, services.window(), services);
    }

    void DemoGame::on_update(const RelayCore::FrameInfo& frame, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_update");

        player_controller_.update(frame, level_);
        player_controller_.apply_camera(services.camera());
        {
            RELAYCORE_PROFILE_ZONE_N("DemoGame::player_diagnostics_publish");
            RelayCore::render::PlayerDiagnostics player_diagnostics{};
            player_diagnostics.position = player_controller_.position();
            player_diagnostics.velocity = player_controller_.velocity();
            player_diagnostics.grounded = player_controller_.grounded();
            player_diagnostics.collision_enabled =
                (player_controller_.movement_mode() != RelayGame::MovementMode::Noclip);
            player_diagnostics.movement_mode = static_cast<std::uint32_t>(player_controller_.movement_mode());

            const auto& velocity     = player_diagnostics.velocity;
            player_diagnostics.speed = std::sqrt((velocity.x * velocity.x) + (velocity.z * velocity.z));
            services.renderer().set_player_diagnostics(player_diagnostics);
        }
    }

    void DemoGame::on_render(const RelayCore::FrameInfo& /*frame*/, RelayCore::EngineServices& services)
    {
        RELAYCORE_PROFILE_ZONE_N("DemoGame::on_render");

        visible_solids_       = 0;
        draw_calls_per_frame_ = 0;
        triangles_per_frame_  = 0;

        const ViewFrustum frustum = extract_frustum(services.camera().view_projection_matrix());
        {
            RELAYCORE_PROFILE_ZONE_N("DemoGame::draw_visible_blockout_chunks");

            for (const RenderChunk& chunk: render_chunks_)
            {
                if (!aabb_intersects_frustum(frustum, chunk.mins, chunk.maxs)) { continue; }

                ++visible_solids_;
                ++draw_calls_per_frame_;
                triangles_per_frame_ += chunk.triangle_count;
                services.renderer().draw_color_mesh(chunk.mesh, glm::mat4{1.0F});
            }
        }

        RelayCore::render::WorldDiagnostics world_diagnostics{};
        world_diagnostics.total_solids   = total_solids_;
        world_diagnostics.visible_solids = visible_solids_;
        world_diagnostics.draw_calls     = draw_calls_per_frame_;
        world_diagnostics.triangles      = triangles_per_frame_;
        services.renderer().set_world_diagnostics(world_diagnostics);
    }

    void DemoGame::on_shutdown(RelayCore::EngineServices& /*services*/) noexcept
    {
        RelayCore::log::info("DemoGame shutdown");
    }

    auto make_demo_game() -> std::unique_ptr<RelayCore::AppDelegate> { return std::make_unique<DemoGame>(); }
} // namespace RelayGame
