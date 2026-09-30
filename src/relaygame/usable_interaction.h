#pragma once

#include "relaycore/gameplay/interaction_runtime.h"

namespace RelayGame
{
    [[nodiscard]] auto apply_usable_interaction(
        RelayCore::gameplay::RuntimeUsable& usable,
        std::vector<RelayCore::gameplay::RuntimeDoor>& doors,
        std::vector<RelayCore::gameplay::RuntimeUsable>& usables,
        std::vector<std::string>& inventory_item_ids,
        std::vector<std::string>& activated_target_ids,
        std::vector<std::string>& signaled_target_ids
    ) -> RelayCore::gameplay::InteractionEvent;

} // namespace RelayGame
