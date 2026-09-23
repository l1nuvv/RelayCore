#include "relaygame/usable_interaction.h"

#include "relaycore/core/profiling.h"
#include "relaycore/gameplay/interaction_runtime_detail.h"

namespace RelayGame
{
    using namespace RelayCore::gameplay;
    namespace
    {
        void record_signaled_target(
            const std::string_view target,
            const bool signaled,
            std::vector<std::string>& signaled_target_ids
        )
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (!signaled)
            {
                return;
            }

            RelayCore::gameplay::detail::append_unique_string(signaled_target_ids, target);
        }

        [[nodiscard]] auto make_usable_event(const RuntimeUsable& usable, const UseInteractionOutcome kind)
            -> InteractionEvent
        {
            RELAYCORE_PROFILE_FUNCTION();
            InteractionEvent event{};
            event.kind = kind;
            event.source_kind = InteractionEventSourceKind::Usable;
            event.source_name = usable.name;
            event.source_label = usable.label;
            event.target = usable.target;
            event.chain_target = usable.chain_target;
            event.item_id = usable.item_id;
            return event;
        }
    } // namespace
    auto apply_usable_interaction(
        RuntimeUsable& usable,
        std::vector<RuntimeDoor>& doors,
        std::vector<RuntimeUsable>& usables,
        std::vector<std::string>& inventory_item_ids,
        std::vector<std::string>& activated_target_ids,
        std::vector<std::string>& signaled_target_ids
    ) -> InteractionEvent
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!usable.enabled || usable.consumed)
        {
            return {};
        }

        if (usable.locked)
        {
            return make_usable_event(usable, UseInteractionOutcome::Locked);
        }

        const bool has_required_item =
            usable.required_item.empty()
            || RelayCore::gameplay::detail::contains_string(inventory_item_ids, usable.required_item);
        usable.requirement_met = has_required_item;
        if (!has_required_item)
        {
            return make_usable_event(usable, UseInteractionOutcome::MissingRequiredItem);
        }

        if (usable.action_kind == UsableActionKind::PickupItem)
        {
            InteractionEvent event = make_usable_event(usable, UseInteractionOutcome::PickedUpItem);
            RelayCore::gameplay::detail::append_unique_string(inventory_item_ids, usable.item_id);
            usable.consumed = true;
            usable.enabled = false;
            event.primary_applied = true;
            event.source_consumed = true;
            if (!usable.chain_target.empty())
            {
                event.chain_applied = signal_target(usable.chain_target, doors, usables);
                record_signaled_target(usable.chain_target, event.chain_applied, signaled_target_ids);
            }
            return event;
        }

        if (!perform_usable_action(usable, doors, usables))
        {
            return {};
        }

        InteractionEvent event = make_usable_event(
            usable,
            usable.action_kind == UsableActionKind::SignalTarget ? UseInteractionOutcome::SignaledTarget
                                                                 : UseInteractionOutcome::ActivatedTarget
        );
        event.primary_applied = true;
        if (usable.action_kind == UsableActionKind::SignalTarget)
        {
            record_signaled_target(usable.target, true, signaled_target_ids);
        } else
        {
            RelayCore::gameplay::detail::append_unique_string(activated_target_ids, usable.target);
        }
        if (!usable.chain_target.empty())
        {
            event.chain_applied = signal_target(usable.chain_target, doors, usables);
            record_signaled_target(usable.chain_target, event.chain_applied, signaled_target_ids);
        }
        if (usable.single_use)
        {
            usable.consumed = true;
            usable.enabled = false;
            event.source_consumed = true;
        }
        return event;
    }

} // namespace RelayGame
