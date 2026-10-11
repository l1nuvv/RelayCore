#pragma once

#include <filesystem>
#include <fstream>

namespace RelayCore::test
{
    // The bench is vertically scaled to make its native opening traversable by a standing player.
    inline void write_native_collision_fixture(const std::filesystem::path& path)
    {
        std::ofstream{path} << R"(
[[solid]]
origin = [0, -0.1, 3]
size = [8, 0.2, 10]
material = "Floor"
[[solid]]
origin = [2.5, 0.01, 0]
size = [1, 0.02, 1]
material = "Hazard"
[[entity]]
classname = "player_spawn"
origin = [0, 0.001, 0]
angles = [0, 0, 0]
[[entity]]
classname = "level_entry"
targetname = "arrival"
origin = [0, 0.001, 0]
angles = [0, 0, 0]
[[entity]]
classname = "prop_static"
origin = [0, 0, 3]
scale = [1, 3, 1]
model = "models/lab_bench.rcmodel"
collision_asset = "collision/lab_bench.rccollision"
[[entity]]
classname = "gameplay_entity"
kind = "door"
targetname = "fixture_door"
origin = [0, 0, 5]
size = [1.4, 2.4, 0.12]
move_offset = [2, 0, 0]
move_speed = 4
[[entity]]
classname = "gameplay_entity"
kind = "panel"
origin = [0, 1.2, 4]
size = [0.2, 0.4, 0.1]
target = "fixture_door"
use_radius = 1.8
label = "Open fixture door"
)";
    }
} // namespace RelayCore::test
