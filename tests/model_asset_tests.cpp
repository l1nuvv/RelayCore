#include "relaycore/core/paths.h"
#include "relaycore/render/model_asset.h"
#include "relaycore/render/static_model.h"
#include "relaygame/level_editor_document.h"
#include "relaygame/static_props.h"

#include <doctest/doctest.h>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <limits>

#include "scoped_temp_directory.h"

namespace
{
    using namespace RelayCore::render;
    auto triangle_model() -> ModelAssetData
    {
        ModelAssetData model;
        model.meshes.push_back(
            {{{{{0, 0, 0}, {1, 1, 1}, {0, 0, 1}, 0, {0, 0}},
               {{1, 0, 0}, {1, 1, 1}, {0, 0, 1}, 0, {1, 0}},
               {{0, 1, 0}, {1, 1, 1}, {0, 0, 1}, 0, {0, 1}}},
              {0, 1, 2}}}
        );
        model.materials.push_back({{1, 1, {40, 80, 160, 255}}});
        model.parts.push_back({0, 0, glm::translate(glm::mat4{1}, {3, 2, 1})});
        return model;
    }

    auto read_bytes(const std::filesystem::path& path) -> std::string
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }
} // namespace

TEST_CASE("native model product is deterministic and preserves geometry/material/transform")
{
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "model.rcmodel";
    auto source = triangle_model();
    source.meshes[0].has_tangents = true;
    for (auto& vertex : source.meshes[0].geometry.vertices)
        vertex.tangent = {1, 0, 0, -1};
    source.materials[0].normal = CpuRgbaTextureData{1, 1, {128, 190, 240, 42}};
    source.materials[0].normal_scale = .4F;
    source.materials[0].metallic_roughness = MetallicRoughness{.3F, .8F};
    write_model_asset(path, source);
    const auto first = read_bytes(path);
    const auto model = read_model_asset(path);
    CHECK(model.meshes[0].geometry.indices == source.meshes[0].geometry.indices);
    CHECK(model.meshes[0].geometry.vertices[1].uv == glm::vec2{1, 0});
    CHECK(model.materials[0].base_color.pixels == source.materials[0].base_color.pixels);
    CHECK(model.meshes[0].has_tangents);
    CHECK(model.meshes[0].geometry.vertices[1].tangent == glm::vec4{1, 0, 0, -1});
    CHECK(model.materials[0].normal->pixels == source.materials[0].normal->pixels);
    CHECK(model.materials[0].normal_scale == .4F);
    CHECK(model.materials[0].metallic_roughness == source.materials[0].metallic_roughness);
    CHECK(model.parts[0].transform[3] == glm::vec4{3, 2, 1, 1});
    write_model_asset(path, model);
    CHECK(read_bytes(path) == first);
    for (const auto length : {std::size_t{0}, std::size_t{19}, first.size() - 1})
    {
        std::ofstream(path, std::ios::binary).write(first.data(), static_cast<std::streamsize>(length));
        CHECK_THROWS_WITH_AS(
            static_cast<void>(read_model_asset(path)),
            doctest::Contains("model.rcmodel"),
            std::runtime_error
        );
    }
    auto changed = first;
    changed[4] = 4;
    std::ofstream(path, std::ios::binary).write(changed.data(), static_cast<std::streamsize>(changed.size()));
    CHECK_THROWS_WITH_AS(static_cast<void>(read_model_asset(path)), doctest::Contains("version"), std::runtime_error);
    std::ofstream(path, std::ios::binary).write((first + "x").data(), static_cast<std::streamsize>(first.size() + 1));
    CHECK_THROWS_WITH_AS(
        static_cast<void>(read_model_asset(path)),
        doctest::Contains("unexpected bytes"),
        std::runtime_error
    );
}

TEST_CASE("native model validation rejects invalid geometry/material/part before writing")
{
    const auto source = triangle_model();
    auto centimetres = source;
    centimetres.parts[0].transform = glm::scale(glm::mat4{1}, glm::vec3{.01F});
    CHECK_NOTHROW(validate_model_asset(centimetres));
    auto normal_mapped = source;
    normal_mapped.materials[0].normal = CpuRgbaTextureData{1, 1, {128, 128, 255, 255}};
    CHECK_THROWS_WITH_AS(
        validate_model_asset(normal_mapped),
        doctest::Contains("requires tangents"),
        std::runtime_error
    );
    normal_mapped.meshes[0].has_tangents = true;
    for (auto& vertex : normal_mapped.meshes[0].geometry.vertices)
        vertex.tangent = {1, 0, 0, 1};
    CHECK_NOTHROW(validate_model_asset(normal_mapped));
    normal_mapped.meshes[0].geometry.vertices[0].tangent.w = -1;
    CHECK_THROWS_WITH_AS(validate_model_asset(normal_mapped), doctest::Contains("handedness"), std::runtime_error);
    normal_mapped.meshes[0].geometry.vertices[0].tangent = {0, 0, 1, 1};
    CHECK_THROWS_WITH_AS(validate_model_asset(normal_mapped), doctest::Contains("perpendicular"), std::runtime_error);
    for (const auto value : {-1.0F, 1.01F, std::numeric_limits<float>::quiet_NaN()})
    {
        auto material = source;
        material.materials[0].metallic_roughness = MetallicRoughness{value, 0};
        CHECK_THROWS_WITH_AS(validate_model_asset(material), doctest::Contains("factors"), std::runtime_error);
        material.materials[0].metallic_roughness = MetallicRoughness{.5F, value};
        CHECK_THROWS_WITH_AS(validate_model_asset(material), doctest::Contains("factors"), std::runtime_error);
    }
    auto check = [&](const auto& corrupt) {
        auto model = source;
        corrupt(model);
        CHECK_THROWS_AS(validate_model_asset(model), std::runtime_error);
    };
    check([](auto& model) {
        model.meshes[0].geometry.indices[2] = 3;
    });
    check([](auto& model) {
        model.meshes[0].geometry.indices[2] = 1;
    });
    check([](auto& model) {
        model.meshes[0].geometry.vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
    });
    check([](auto& model) {
        model.meshes[0].geometry.vertices[0].normal = glm::vec3{0};
    });
    check([](auto& model) {
        model.materials[0].base_color.pixels.pop_back();
    });
    check([](auto& model) {
        model.materials[0].base_color.pixels[3] = 0;
    });
    check([](auto& model) {
        model.parts[0].mesh = 1;
    });
    check([](auto& model) {
        model.parts[0].material = 1;
    });
    check([](auto& model) {
        model.parts[0].transform[0][0] = 0;
    });
}

TEST_CASE("native model cache shares resources and keeps old owners through failed reload")
{
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "model.rcmodel";
    auto data = triangle_model();
    write_model_asset(path, data);
    StaticModelCache cache;
    const auto first = cache.load(path, TextureEncoding::srgb);
    CHECK(cache.load(path, TextureEncoding::srgb) == first);
    CHECK(first->mins == glm::vec3{3, 2, 1});
    CHECK(first->maxs == glm::vec3{4, 3, 1});
    CHECK(cache.load(path, TextureEncoding::linear) != first);
    RelayCore::BlockoutLevel level;
    RelayCore::EntitySpawnData entity;
    entity.classname = "prop_static";
    entity.properties = {{"model", path.generic_string()}};
    level.entities.push_back(entity);
    RelayGame::LevelEditorDocument editor;
    editor.reset(level);
    CHECK(editor.bounds({true, 0}).maxs.x == 4);
    data.parts.push_back({0, 0, glm::translate(glm::mat4{1}, {10, 0, 0})});
    write_model_asset(path, data);
    const auto second = cache.load(path, TextureEncoding::srgb);
    CHECK(second != first);
    CHECK(first->parts.size() == 1);
    CHECK(second->parts.size() == 2);
    editor.reset(level);
    CHECK(editor.bounds({true, 0}).maxs.x == 11);
    std::ofstream(path) << "broken";
    CHECK_THROWS(static_cast<void>(cache.load(path, TextureEncoding::srgb)));
    CHECK_THROWS(editor.reset(level));
    CHECK(editor.bounds({true, 0}).maxs.x == 11);
    CHECK(second->meshes[0].index_count() == 3);
    CHECK(first->materials[0].base_color.cpu_data()->pixels[2] == 160);
}

TEST_CASE("native static prop and editor use model bounds and embedded materials")
{
    RelayCore::BlockoutLevel level;
    RelayCore::EntitySpawnData entity;
    entity.classname = "prop_static";
    entity.properties = {{"model", "models/service_cylinder.rcmodel"}};
    level.entities.push_back(entity);
    const auto props = RelayGame::build_static_props(level);
    REQUIRE(props.size() == 1);
    CHECK(props[0].texture.empty());
    level.entities[0].properties["texture"] = "unused.bmp";
    CHECK_THROWS_WITH_AS(
        static_cast<void>(RelayGame::build_static_props(level)),
        doctest::Contains("remove texture"),
        std::runtime_error
    );
    RelayGame::LevelEditorDocument editor;
    editor.reset({});
    editor.add_model({3, 0, 5}, "models/service_cylinder.rcmodel", "");
    REQUIRE(editor.level().entities.size() == 1);
    CHECK_FALSE(editor.level().entities[0].properties.contains("texture"));
    editor.set_model_collision(true);
    const auto placed = RelayGame::build_static_props(editor.level());
    REQUIRE(placed[0].collider.has_value());
    CHECK(placed[0].collider->maxs.y == doctest::Approx(1.51).epsilon(.01));
}

TEST_CASE("native v1 product remains readable as a base-color-only model")
{
    constexpr std::string_view hex = "524d444c01000000010000000100000001000000030000000300000000000000"
                                     "00000000000000000000803f0000803f0000803f00000000000000000000803f"
                                     "0000000000000000000000000000803f00000000000000000000803f0000803f"
                                     "0000803f00000000000000000000803f000000000000803f0000000000000000"
                                     "0000803f000000000000803f0000803f0000803f00000000000000000000803f"
                                     "00000000000000000000803f0000000001000000020000000100000001000000"
                                     "2850a0ff00000000000000000000803f00000000000000000000000000000000"
                                     "0000803f000000000000000000000000000000000000803f0000000000004040"
                                     "000000400000803f0000803f";
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "v1.rcmodel";
    std::ofstream file(path, std::ios::binary);
    for (std::size_t i = 0; i < hex.size(); i += 2)
        file.put(static_cast<char>(std::stoi(std::string{hex.substr(i, 2)}, nullptr, 16)));
    file.close();
    const auto model = read_model_asset(path);
    CHECK_FALSE(model.meshes[0].has_tangents);
    CHECK_FALSE(model.materials[0].normal);
    CHECK(model.meshes[0].geometry.vertices[1].position == glm::vec3{1, 0, 0});
    CHECK(model.materials[0].base_color.pixels == std::vector<std::uint8_t>{40, 80, 160, 255});
    CHECK(model.parts[0].transform[3] == glm::vec4{3, 2, 1, 1});
}

TEST_CASE("native v2 normal product keeps its original lighting contract")
{
    constexpr std::string_view hex = "524d444c02000000010000000100000001000000010000000300000003000000"
                                     "0000000000000000000000000000803f0000803f0000803f0000000000000000"
                                     "0000803f0000000000000000000000000000803f00000000000000000000803f"
                                     "0000803f00000000000000000000803f0000803f0000803f0000000000000000"
                                     "0000803f000000000000803f000000000000803f00000000000000000000803f"
                                     "000000000000803f000000000000803f0000803f0000803f0000000000000000"
                                     "0000803f00000000000000000000803f0000803f00000000000000000000803f"
                                     "00000000010000000200000001000000010000002850a0ff0100000001000000"
                                     "cdcccc3e80bef0ff00000000000000000000803f000000000000000000000000"
                                     "000000000000803f000000000000000000000000000000000000803f00000000"
                                     "00004040000000400000803f0000803f";
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "v2.rcmodel";
    std::ofstream file(path, std::ios::binary);
    for (std::size_t i = 0; i < hex.size(); i += 2)
        file.put(static_cast<char>(std::stoi(std::string{hex.substr(i, 2)}, nullptr, 16)));
    file.close();
    const auto model = read_model_asset(path);
    CHECK_FALSE(model.materials[0].metallic_roughness);
    CHECK(model.meshes[0].has_tangents);
    CHECK(model.materials[0].normal->pixels == std::vector<std::uint8_t>{128, 190, 240, 255});
    CHECK(model.materials[0].normal_scale == .4F);
    write_model_asset(path, model);
    CHECK_FALSE(read_model_asset(path).materials[0].metallic_roughness);
}
