#pragma once

#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/render/render_queue.h"

#include <array>
#include <cstdint>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <span>
#include <vector>

namespace RelayCore
{
    struct BlockoutLevel;
    class Renderer;
} // namespace RelayCore

namespace RelayCore::render
{
    class PerspectiveCamera;
}

namespace RelayGame
{
    class WorldSceneRenderer {
    public:
        void initialize(const RelayCore::BlockoutLevel& level);

        void shutdown() noexcept;

        void render(
            RelayCore::Renderer& renderer,
            const RelayCore::render::PerspectiveCamera& camera,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors,
            std::span<const RelayCore::gameplay::RuntimeUsable> usables,
            int focused_usable_index
        );

        [[nodiscard]] auto diagnostics() const noexcept -> const RelayCore::render::WorldDiagnostics&;

    private:
        void render_doors(
            const RelayCore::render::PerspectiveCamera& camera,
            std::span<const RelayCore::gameplay::RuntimeDoor> doors
        );

        void render_interactive_usables(
            const RelayCore::render::PerspectiveCamera& camera,
            std::span<const RelayCore::gameplay::RuntimeUsable> usables,
            int focused_usable_index
        );

        struct RenderChunk {
            RelayCore::render::StaticColorMesh mesh{};
            glm::vec3 mins{};
            glm::vec3 maxs{};
            std::uint32_t triangle_count = 0;
            bool transparent = false;
        };

        std::array<glm::vec4, 6> frustum_planes_{};
        RelayCore::render::StaticColorMesh door_mesh_{};
        RelayCore::render::StaticColorMesh panel_mesh_{};
        RelayCore::render::StaticColorMesh panel_highlight_mesh_{};
        RelayCore::render::StaticColorMesh panel_success_mesh_{};
        RelayCore::render::StaticColorMesh panel_warning_mesh_{};
        RelayCore::render::StaticColorMesh pickup_mesh_{};
        RelayCore::render::StaticColorMesh pickup_highlight_mesh_{};
        std::vector<RenderChunk> render_chunks_{};
        std::vector<RelayCore::render::LocalLight> local_lights_{};
        RelayCore::render::RenderQueue render_queue_{};
        RelayCore::render::WorldDiagnostics diagnostics_{};
    };
} // namespace RelayGame
