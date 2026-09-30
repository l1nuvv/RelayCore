#include "relaygame/diagnostics_publication_policy.h"

#include <doctest/doctest.h>

TEST_CASE("diagnostics publication policy skips optional snapshots when every consumer is hidden")
{
    const RelayGame::DiagnosticsPublicationNeeds needs = RelayGame::diagnostics_publication_needs(false, false, false);

    CHECK_FALSE(needs.runtime_and_audio);
    CHECK_FALSE(needs.inspector);
}

TEST_CASE("diagnostics publication policy serves the F2 overlay")
{
    const RelayGame::DiagnosticsPublicationNeeds needs = RelayGame::diagnostics_publication_needs(true, false, false);

    CHECK(needs.runtime_and_audio);
    CHECK_FALSE(needs.inspector);
}

TEST_CASE("diagnostics publication policy serves advanced F6 diagnostics")
{
    const RelayGame::DiagnosticsPublicationNeeds needs = RelayGame::diagnostics_publication_needs(false, true, false);

    CHECK(needs.runtime_and_audio);
    CHECK(needs.inspector);
}

TEST_CASE("diagnostics publication policy serves the F4 inspector visualization")
{
    const RelayGame::DiagnosticsPublicationNeeds needs = RelayGame::diagnostics_publication_needs(false, false, true);

    CHECK_FALSE(needs.runtime_and_audio);
    CHECK(needs.inspector);
}

TEST_CASE("diagnostics publication policy combines F2 and F4 consumers")
{
    const RelayGame::DiagnosticsPublicationNeeds needs = RelayGame::diagnostics_publication_needs(true, false, true);

    CHECK(needs.runtime_and_audio);
    CHECK(needs.inspector);
}
