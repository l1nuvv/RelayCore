#pragma once

#include "relaycore/gameplay/interaction_runtime.h"
#include "relaycore/gameplay/runtime_model.h"
#include "relaycore/render/render_diagnostics.h"
#include "relaycore/world/blockout_level.h"

#include <glm/vec3.hpp>
#include <span>
#include <string_view>

namespace RelayGame
{
    [[nodiscard]] auto build_world_inspector_diagnostics(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const glm::vec3& eye_position,
        const glm::vec3& view_forward,
        float max_distance = 24.0F
    ) -> RelayCore::render::InspectorDiagnostics;

    [[nodiscard]] auto build_world_inspector_diagnostics(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const RelayCore::gameplay::RuntimeObjectRegistry& registry,
        const glm::vec3& eye_position,
        const glm::vec3& view_forward,
        float max_distance = 24.0F
    ) -> RelayCore::render::InspectorDiagnostics;

    [[nodiscard]] auto find_world_inspector_diagnostics_by_authored_source(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        std::string_view object_kind,
        std::string_view primary_source_reference,
        const glm::vec3& eye_position
    ) -> RelayCore::render::InspectorDiagnostics;

    [[nodiscard]] auto find_world_inspector_diagnostics_by_runtime_id(
        const RelayCore::BlockoutLevel& level,
        std::span<const RelayCore::gameplay::RuntimeDoor> doors,
        std::span<const RelayCore::gameplay::RuntimeTrigger> triggers,
        std::span<const RelayCore::gameplay::RuntimeUsable> usables,
        const RelayCore::gameplay::RuntimeObjectRegistry& registry,
        RelayCore::gameplay::RuntimeObjectId runtime_id,
        const glm::vec3& eye_position
    ) -> RelayCore::render::InspectorDiagnostics;
} // namespace RelayGame
