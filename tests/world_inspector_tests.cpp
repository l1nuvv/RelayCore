#include "relaycore/physics/collision.h"
#include "relaycore/world/blockout_level.h"
#include "relaygame/world_inspector.h"

#include <doctest/doctest.h>
#include <string>
#include <vector>

TEST_CASE("ray intersection distance reports hit miss and inside cases")
{
    const RelayCore::physics::Aabb bounds{
        .mins = {-1.0F, 0.0F, 4.0F},
        .maxs = {1.0F, 2.0F, 6.0F},
    };

    const auto front_hit =
        RelayCore::physics::ray_intersection_distance({0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, bounds, 10.0F);
    REQUIRE(front_hit.has_value());
    CHECK(*front_hit == doctest::Approx(4.0F));

    const auto inside_hit =
        RelayCore::physics::ray_intersection_distance({0.0F, 1.0F, 5.0F}, {0.0F, 0.0F, 1.0F}, bounds, 10.0F);
    REQUIRE(inside_hit.has_value());
    CHECK(*inside_hit == doctest::Approx(0.0F));

    const auto miss =
        RelayCore::physics::ray_intersection_distance({2.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, bounds, 10.0F);
    CHECK_FALSE(miss.has_value());
}

TEST_CASE("world inspector prefers usable over solid behind it")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "logic_usable",
        .origin = {0.0F, 1.2F, 1.1F},
        .properties =
            {
                {"kind", "panel"},
                {"targetname", "lab_reader"},
                {"target", "inner_bulkhead"},
            },
    });
    level.entity_sources.push_back(RelayCore::AuthoredSourceLocation{
        .file_path = "tests/data/world_inspector_panel.toml",
        .line = 7,
        .column = 1,
    });
    level.solids.push_back(RelayCore::BlockoutSolid{
        .mins = {-1.0F, 0.0F, 2.0F},
        .maxs = {1.0F, 2.0F, 2.2F},
        .material = RelayCore::SurfaceMaterial::Wall,
    });

    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    std::vector<RelayCore::gameplay::RuntimeTrigger> triggers;
    std::vector<RelayCore::gameplay::RuntimeUsable> usables{
        RelayCore::gameplay::RuntimeUsable{
            .authored_entity_index = 0,
            .name = "runtime_panel_proxy",
            .label = "Lab reader",
            .target = "inner_bulkhead",
            .origin = {0.0F, 1.2F, 1.8F},
            .visual_bounds =
                RelayCore::physics::Aabb{
                    .mins = {-0.2F, 0.9F, 1.7F},
                    .maxs = {0.2F, 1.5F, 1.9F},
                },
            .use_radius = 2.5F,
            .has_visual_bounds = true,
            .enabled = true,
            .visual_kind = RelayCore::gameplay::UsableVisualKind::Panel,
            .panel_mount_axis = RelayCore::gameplay::PanelMountAxis::NegativeZ,
            .action_kind = RelayCore::gameplay::UsableActionKind::ActivateTarget,
        },
    };

    const RelayCore::render::InspectorDiagnostics diagnostics = RelayGame::build_world_inspector_diagnostics(
        level,
        doors,
        triggers,
        usables,
        {0.0F, 1.2F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        8.0F
    );

    REQUIRE(diagnostics.has_pick);
    CHECK(diagnostics.object_kind == "usable");
    CHECK(diagnostics.name == "runtime_panel_proxy");
    CHECK(diagnostics.target == "inner_bulkhead");
    CHECK(diagnostics.visual_kind == "panel");
    CHECK(diagnostics.panel_mount_axis == "-z");
    CHECK(diagnostics.distance == doctest::Approx(1.7F));
    CHECK(diagnostics.has_authored_source);
    CHECK(diagnostics.authored_source_kind == "entity");
    CHECK(diagnostics.authored_index == 0);
    CHECK(diagnostics.authored_classname == "logic_usable");
    CHECK(diagnostics.authored_entity_kind == "panel");
    CHECK(diagnostics.authored_targetname == "lab_reader");
    CHECK(diagnostics.has_authored_location);
    CHECK(diagnostics.authored_file_path == "tests/data/world_inspector_panel.toml");
    CHECK(diagnostics.authored_line == 7);
    CHECK(diagnostics.authored_column == 1);
}

TEST_CASE("world inspector reports instanced authored provenance for runtime picks")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "logic_usable",
        .origin = {0.0F, 1.2F, 1.1F},
        .properties =
            {
                {"kind", "panel"},
                {"targetname", "templated_reader"},
                {"target", "inner_bulkhead"},
            },
    });
    level.entity_sources.push_back(RelayCore::AuthoredSourceLocation{
        .file_path = "tests/data/world_inspector_template.toml",
        .line = 14,
        .column = 3,
        .source_kind = "module_instance",
        .template_name = "bulkhead_reader",
        .instance_name = "lab_entry",
        .placement_file_path = "tests/data/world_inspector_level.toml",
        .placement_line = 42,
        .placement_column = 1,
    });

    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    std::vector<RelayCore::gameplay::RuntimeTrigger> triggers;
    std::vector<RelayCore::gameplay::RuntimeUsable> usables{
        RelayCore::gameplay::RuntimeUsable{
            .authored_entity_index = 0,
            .name = "templated_reader_runtime",
            .label = "Templated reader",
            .target = "inner_bulkhead",
            .origin = {0.0F, 1.2F, 1.8F},
            .visual_bounds =
                RelayCore::physics::Aabb{
                    .mins = {-0.2F, 0.9F, 1.7F},
                    .maxs = {0.2F, 1.5F, 1.9F},
                },
            .use_radius = 2.5F,
            .has_visual_bounds = true,
            .enabled = true,
            .visual_kind = RelayCore::gameplay::UsableVisualKind::Panel,
            .panel_mount_axis = RelayCore::gameplay::PanelMountAxis::NegativeZ,
            .action_kind = RelayCore::gameplay::UsableActionKind::ActivateTarget,
        },
    };

    const RelayCore::render::InspectorDiagnostics diagnostics = RelayGame::build_world_inspector_diagnostics(
        level,
        doors,
        triggers,
        usables,
        {0.0F, 1.2F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        8.0F
    );

    REQUIRE(diagnostics.has_pick);
    CHECK(diagnostics.object_kind == "usable");
    CHECK(diagnostics.has_authored_source);
    CHECK(diagnostics.authored_source_kind == "entity");
    CHECK(diagnostics.has_authored_location);
    CHECK(diagnostics.authored_file_path == "tests/data/world_inspector_template.toml");
    CHECK(diagnostics.authored_line == 14);
    CHECK(diagnostics.authored_column == 3);
    CHECK(diagnostics.has_authored_placement_location);
    CHECK(diagnostics.authored_placement_kind == "module_instance");
    CHECK(diagnostics.authored_template_name == "bulkhead_reader");
    CHECK(diagnostics.authored_instance_name == "lab_entry");
    CHECK(diagnostics.authored_placement_file_path == "tests/data/world_inspector_level.toml");
    CHECK(diagnostics.authored_placement_line == 42);
    CHECK(diagnostics.authored_placement_column == 1);
}

TEST_CASE("world inspector includes runtime handle when registry is available")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "logic_usable",
        .origin = {0.0F, 1.2F, 1.1F},
        .properties =
            {
                {"kind", "panel"},
                {"targetname", "handle_reader"},
                {"target", "inner_bulkhead"},
            },
    });
    level.entity_sources.push_back(RelayCore::AuthoredSourceLocation{
        .file_path = "tests/data/world_inspector_handle.toml",
        .line = 6,
        .column = 1,
    });

    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    std::vector<RelayCore::gameplay::RuntimeTrigger> triggers;
    std::vector<RelayCore::gameplay::RuntimeUsable> usables{
        RelayCore::gameplay::RuntimeUsable{
            .authored_entity_index = 0,
            .name = "handle_reader_runtime",
            .label = "Handle reader",
            .target = "inner_bulkhead",
            .origin = {0.0F, 1.2F, 1.8F},
            .visual_bounds =
                RelayCore::physics::Aabb{
                    .mins = {-0.2F, 0.9F, 1.7F},
                    .maxs = {0.2F, 1.5F, 1.9F},
                },
            .use_radius = 2.5F,
            .has_visual_bounds = true,
            .enabled = true,
            .visual_kind = RelayCore::gameplay::UsableVisualKind::Panel,
            .panel_mount_axis = RelayCore::gameplay::PanelMountAxis::NegativeZ,
            .action_kind = RelayCore::gameplay::UsableActionKind::ActivateTarget,
        },
    };

    RelayGame::RuntimeObjectRegistry registry{};
    registry.build(level, doors, triggers, usables);

    const RelayCore::render::InspectorDiagnostics diagnostics = RelayGame::build_world_inspector_diagnostics(
        level,
        doors,
        triggers,
        usables,
        registry,
        {0.0F, 1.2F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        8.0F
    );

    REQUIRE(diagnostics.has_pick);
    CHECK(diagnostics.has_runtime_handle);
    CHECK_FALSE(diagnostics.runtime_handle.empty());
    CHECK(diagnostics.runtime_id != 0);
    CHECK(diagnostics.runtime_handle.find("handle_reader_runtime") != std::string::npos);
}

TEST_CASE("world inspector reports door runtime state")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "func_door",
        .origin = {0.0F, 1.25F, 3.1F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "bulkhead_alpha"},
            },
    });
    level.entity_sources.push_back(RelayCore::AuthoredSourceLocation{
        .file_path = "tests/data/world_inspector_door.toml",
        .line = 11,
        .column = 1,
    });
    std::vector<RelayCore::gameplay::RuntimeDoor> doors{
        RelayCore::gameplay::RuntimeDoor{
            .authored_entity_index = 0,
            .name = "runtime_bulkhead_proxy",
            .closed_bounds =
                RelayCore::physics::Aabb{
                    .mins = {-1.0F, 0.0F, 3.4F},
                    .maxs = {1.0F, 2.5F, 3.6F},
                },
            .open_offset = {0.0F, 0.0F, 0.0F},
            .open_fraction = 0.35F,
            .blocks_when_closed = true,
            .state = RelayCore::gameplay::DoorState::Opening,
        },
    };
    std::vector<RelayCore::gameplay::RuntimeTrigger> triggers;
    std::vector<RelayCore::gameplay::RuntimeUsable> usables;

    const RelayCore::render::InspectorDiagnostics diagnostics = RelayGame::build_world_inspector_diagnostics(
        level,
        doors,
        triggers,
        usables,
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        8.0F
    );

    REQUIRE(diagnostics.has_pick);
    CHECK(diagnostics.object_kind == "door");
    CHECK(diagnostics.name == "runtime_bulkhead_proxy");
    CHECK(diagnostics.state == "opening");
    CHECK(diagnostics.blocks_when_closed);
    CHECK(diagnostics.door_open_fraction == doctest::Approx(0.35F));
    CHECK(diagnostics.has_authored_source);
    CHECK(diagnostics.authored_source_kind == "entity");
    CHECK(diagnostics.authored_classname == "func_door");
    CHECK(diagnostics.authored_targetname == "bulkhead_alpha");
    CHECK(diagnostics.has_authored_location);
    CHECK(diagnostics.authored_file_path == "tests/data/world_inspector_door.toml");
    CHECK(diagnostics.authored_line == 11);
}

TEST_CASE("world inspector reports trigger state before solid fallback")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "trigger_volume",
        .origin = {4.0F, 0.0F, 4.0F},
        .properties =
            {
                {"kind", "trigger"},
                {"targetname", "trigger_authored"},
            },
    });
    level.entity_sources.push_back(RelayCore::AuthoredSourceLocation{
        .file_path = "tests/data/world_inspector_trigger.toml",
        .line = 9,
        .column = 1,
    });
    level.solids.push_back(RelayCore::BlockoutSolid{
        .mins = {-1.0F, 0.0F, 3.0F},
        .maxs = {1.0F, 2.0F, 3.2F},
        .material = RelayCore::SurfaceMaterial::Hazard,
        .brightness = 1.2F,
    });

    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    std::vector<RelayCore::gameplay::RuntimeTrigger> triggers{
        RelayCore::gameplay::RuntimeTrigger{
            .authored_entity_index = 0,
            .name = "runtime_trigger_proxy",
            .target = "lab_terminal",
            .bounds =
                RelayCore::physics::Aabb{
                    .mins = {-0.5F, 0.0F, 1.5F},
                    .maxs = {0.5F, 2.0F, 1.7F},
                },
            .once = true,
            .fired = true,
            .action_kind = RelayCore::gameplay::TriggerActionKind::SignalTarget,
        },
    };
    std::vector<RelayCore::gameplay::RuntimeUsable> usables;

    const RelayCore::render::InspectorDiagnostics diagnostics = RelayGame::build_world_inspector_diagnostics(
        level,
        doors,
        triggers,
        usables,
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        8.0F
    );

    REQUIRE(diagnostics.has_pick);
    CHECK(diagnostics.object_kind == "trigger");
    CHECK(diagnostics.name == "runtime_trigger_proxy");
    CHECK(diagnostics.target == "lab_terminal");
    CHECK(diagnostics.action_kind == "signal_target");
    CHECK(diagnostics.once);
    CHECK(diagnostics.fired);
    CHECK(diagnostics.has_authored_source);
    CHECK(diagnostics.authored_source_kind == "entity");
    CHECK(diagnostics.authored_index == 0);
    CHECK(diagnostics.authored_targetname == "trigger_authored");
    CHECK(diagnostics.authored_file_path == "tests/data/world_inspector_trigger.toml");
}

TEST_CASE("world inspector falls back to solid material")
{
    RelayCore::BlockoutLevel level{};
    level.solids.push_back(RelayCore::BlockoutSolid{
        .mins = {-1.0F, 0.0F, 2.5F},
        .maxs = {1.0F, 2.0F, 2.7F},
        .material = RelayCore::SurfaceMaterial::Divider,
        .brightness = 1.4F,
    });
    level.solid_sources.push_back(RelayCore::AuthoredSourceLocation{
        .file_path = "tests/data/world_inspector_solid.toml",
        .line = 4,
        .column = 1,
    });
    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    std::vector<RelayCore::gameplay::RuntimeTrigger> triggers;
    std::vector<RelayCore::gameplay::RuntimeUsable> usables;

    const RelayCore::render::InspectorDiagnostics diagnostics = RelayGame::build_world_inspector_diagnostics(
        level,
        doors,
        triggers,
        usables,
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        8.0F
    );

    REQUIRE(diagnostics.has_pick);
    CHECK(diagnostics.object_kind == "solid");
    CHECK(diagnostics.material == "divider");
    CHECK(diagnostics.brightness == doctest::Approx(1.4F));
    CHECK(diagnostics.distance == doctest::Approx(2.5F));
    CHECK(diagnostics.has_authored_source);
    CHECK(diagnostics.authored_source_kind == "solid");
    CHECK(diagnostics.authored_index == 0);
    CHECK(diagnostics.authored_summary == "level.solid[0]");
    CHECK(diagnostics.has_authored_location);
    CHECK(diagnostics.authored_file_path == "tests/data/world_inspector_solid.toml");
    CHECK(diagnostics.authored_line == 4);
    CHECK(diagnostics.authored_column == 1);
}

TEST_CASE("world inspector marks generated door usables without explicit authored entity")
{
    RelayCore::BlockoutLevel level{};
    level.entities.push_back(RelayCore::EntitySpawnData{
        .classname = "func_door",
        .origin = {0.0F, 1.0F, 3.0F},
        .properties =
            {
                {"kind", "door"},
                {"targetname", "bulkhead_beta"},
            },
    });
    level.entity_sources.push_back(RelayCore::AuthoredSourceLocation{
        .file_path = "tests/data/world_inspector_generated.toml",
        .line = 5,
        .column = 1,
    });

    std::vector<RelayCore::gameplay::RuntimeDoor> doors;
    std::vector<RelayCore::gameplay::RuntimeTrigger> triggers;
    std::vector<RelayCore::gameplay::RuntimeUsable> usables{
        RelayCore::gameplay::RuntimeUsable{
            .generated_from_entity_index = 0,
            .name = "",
            .label = "Use",
            .target = "bulkhead_beta",
            .origin = {0.0F, 1.0F, 2.9F},
            .focus_bounds =
                RelayCore::physics::Aabb{
                    .mins = {-0.5F, 0.3F, 2.8F},
                    .maxs = {0.5F, 1.7F, 3.0F},
                },
            .use_radius = 2.5F,
            .has_focus_bounds = true,
            .enabled = true,
            .tracks_target_door = true,
            .action_kind = RelayCore::gameplay::UsableActionKind::ActivateTarget,
        },
    };

    const RelayCore::render::InspectorDiagnostics diagnostics = RelayGame::build_world_inspector_diagnostics(
        level,
        doors,
        triggers,
        usables,
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
        8.0F
    );

    REQUIRE(diagnostics.has_pick);
    CHECK(diagnostics.object_kind == "usable");
    CHECK(diagnostics.has_authored_source);
    CHECK(diagnostics.authored_source_kind == "generated_from_entity");
    CHECK(diagnostics.authored_index == 0);
    CHECK(diagnostics.authored_classname == "func_door");
    CHECK(diagnostics.authored_targetname == "bulkhead_beta");
    CHECK(diagnostics.authored_summary.find("bulkhead_beta") != std::string::npos);
    CHECK(diagnostics.has_authored_location);
    CHECK(diagnostics.authored_file_path == "tests/data/world_inspector_generated.toml");
    CHECK(diagnostics.authored_line == 5);
}
