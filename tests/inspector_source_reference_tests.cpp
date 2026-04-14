#include "relaycore/debug/inspector_source_reference.h"

#include <doctest/doctest.h>

TEST_CASE("inspector source reference prefers placement for instanced content")
{
    RelayCore::render::InspectorDiagnostics inspector{};
    inspector.has_authored_source = true;
    inspector.has_authored_location = true;
    inspector.authored_file_path = "assets/levels/templates/bulkhead.toml";
    inspector.authored_line = 14;
    inspector.authored_column = 3;
    inspector.has_authored_placement_location = true;
    inspector.authored_placement_file_path = "assets/levels/test_room.toml";
    inspector.authored_placement_line = 42;
    inspector.authored_placement_column = 1;

    CHECK(RelayCore::debug::build_primary_inspector_source_reference(inspector) == "assets/levels/test_room.toml:42:1");
    const RelayCore::debug::SourceLocation primary_location =
        RelayCore::debug::build_primary_inspector_source_location(inspector);
    CHECK(primary_location.valid());
    CHECK(primary_location.file_path == "assets/levels/test_room.toml");
    CHECK(primary_location.line == 42);
    CHECK(primary_location.column == 1);
    CHECK(
        RelayCore::debug::build_full_inspector_source_reference(inspector)
        == "placement assets/levels/test_room.toml:42:1 | definition assets/levels/templates/bulkhead.toml:14:3"
    );
}

TEST_CASE("inspector source reference falls back to direct authored definition")
{
    RelayCore::render::InspectorDiagnostics inspector{};
    inspector.has_authored_source = true;
    inspector.has_authored_location = true;
    inspector.authored_file_path = "assets/levels/test_room.toml";
    inspector.authored_line = 77;
    inspector.authored_column = 1;

    CHECK(RelayCore::debug::build_primary_inspector_source_reference(inspector) == "assets/levels/test_room.toml:77:1");
    CHECK(RelayCore::debug::build_full_inspector_source_reference(inspector) == "assets/levels/test_room.toml:77:1");
    CHECK(RelayCore::debug::has_copyable_inspector_source(inspector));
}

TEST_CASE("inspector source reference is empty without authored source")
{
    RelayCore::render::InspectorDiagnostics inspector{};

    CHECK(RelayCore::debug::build_primary_inspector_source_reference(inspector).empty());
    CHECK(RelayCore::debug::build_full_inspector_source_reference(inspector).empty());
    CHECK_FALSE(RelayCore::debug::has_copyable_inspector_source(inspector));
}
