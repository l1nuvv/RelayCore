#include "relaycore/render/shader_source_loader.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include "scoped_temp_directory.h"

namespace
{
    void write_shader_loader_test_file(const std::filesystem::path& path, const std::string_view contents)
    {
        std::ofstream out{path, std::ios::trunc};
        if (!out.is_open())
        {
            throw std::runtime_error("Failed to write shader source loader test file");
        }

        out << contents;
    }
} // namespace

TEST_CASE("shader source loader expands nested local includes and rejects cycles")
{
    RelayCore::test::ScopedTempDirectory temp;
    const std::filesystem::path temp_dir = temp.path() / "relaycore_shader_loader_tests";
    std::filesystem::create_directories(temp_dir / "shared");

    write_shader_loader_test_file(temp_dir / "shared" / "math.glsl", "float included_noise() { return 1.0; }\n");
    write_shader_loader_test_file(
        temp_dir / "shared" / "lighting.glsl",
        "#include \"math.glsl\"\nvec3 included_light() { return vec3(included_noise()); }\n"
    );
    write_shader_loader_test_file(
        temp_dir / "main.frag",
        "#version 460 core\n#include \"shared/lighting.glsl\"\nvoid main() {}\n"
    );

    const std::string expanded = RelayCore::render::load_shader_source(temp_dir / "main.frag");
    CHECK(expanded.find("#version 460 core") != std::string::npos);
    CHECK(expanded.find("included_noise") != std::string::npos);
    CHECK(expanded.find("included_light") != std::string::npos);

    const std::string injected =
        RelayCore::render::load_shader_source(temp_dir / "main.frag", "const int injected_contract = 7;");
    const std::size_t version_pos = injected.find("#version 460 core");
    const std::size_t contract_pos = injected.find("const int injected_contract = 7;");
    const std::size_t include_pos = injected.find("included_light");
    CHECK(version_pos != std::string::npos);
    CHECK(contract_pos != std::string::npos);
    CHECK(include_pos != std::string::npos);
    CHECK(version_pos < contract_pos);
    CHECK(contract_pos < include_pos);
    CHECK(injected.find("included_light") != std::string::npos);

    write_shader_loader_test_file(temp_dir / "cycle_a.glsl", "#include \"cycle_b.glsl\"\n");
    write_shader_loader_test_file(temp_dir / "cycle_b.glsl", "#include \"cycle_a.glsl\"\n");

    const auto load_cycle = [&]() {
        return RelayCore::render::load_shader_source(temp_dir / "cycle_a.glsl");
    };
    CHECK_THROWS_WITH_AS(load_cycle(), doctest::Contains("Shader include cycle detected"), std::runtime_error);
}
