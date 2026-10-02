#include "relaycore/core/paths.h"
#include "relaycore/render/static_model.h"

#include <algorithm>
#include <array>
#include <bit>
#include <doctest/doctest.h>
#include <fstream>
#include <glm/common.hpp>
#include <glm/matrix.hpp>
#include <limits>
#include <tuple>

#include "model_import.h"
#include "scoped_temp_directory.h"

namespace
{
    constexpr auto triangle_json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
"nodes":[{"children":[1,2],"translation":[3,0,0],"rotation":[0,0,0.70710677,0.70710677]},
{"mesh":0,"translation":[2,0,0],"scale":[2,1,1]}, {"mesh":0,"translation":[0,0,2],"scale":[-1,1,1]}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],
"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.25,0.5,1,1],"metallicFactor":0}}],
"buffers":[{"byteLength":96}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}]})";

    constexpr std::array<std::uint8_t, 70> normal_png{137, 80,  78,  71,  13,  10,  26, 10, 0,  0,  0,   13, 73, 72,
                                                      68,  82,  0,   0,   0,   1,   0,  0,  0,  1,  8,   6,  0,  0,
                                                      0,   31,  21,  196, 137, 0,   0,  0,  13, 73, 68,  65, 84, 120,
                                                      156, 99,  104, 56,  240, 225, 63, 0,  7,  36, 3,   48, 85, 205,
                                                      65,  130, 0,   0,   0,   0,   73, 69, 78, 68, 174, 66, 96, 130};

    void
    write_glb(const std::filesystem::path& path, std::string json, const bool nan = false, const bool normal = false)
    {
        while (json.size() % 4)
            json += ' ';
        std::ofstream output(path, std::ios::binary);
        const auto integer = [&](const std::uint32_t value) {
            for (int shift = 0; shift < 32; shift += 8)
                output.put(static_cast<char>(value >> shift));
        };
        integer(0x46546c67);
        integer(2);
        const auto payload_size = normal ? (144 + normal_png.size() + 3) / 4 * 4 : 96;
        integer(static_cast<std::uint32_t>(28 + json.size() + payload_size));
        integer(static_cast<std::uint32_t>(json.size()));
        integer(0x4e4f534a);
        output.write(json.data(), static_cast<std::streamsize>(json.size()));
        integer(static_cast<std::uint32_t>(payload_size));
        integer(0x004e4942);
        std::array<float, 24> values{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
        if (nan)
            values[0] = std::numeric_limits<float>::quiet_NaN();
        for (const auto value : values)
            integer(std::bit_cast<std::uint32_t>(value));
        if (normal)
        {
            for (int i = 0; i < 3; ++i)
                for (const auto value : {1.0F, 0.0F, 0.0F, 1.0F})
                    integer(std::bit_cast<std::uint32_t>(value));
            output.write(reinterpret_cast<const char*>(normal_png.data()), normal_png.size());
            for (std::size_t i = 144 + normal_png.size(); i < payload_size; ++i)
                output.put(0);
        }
    }
} // namespace

TEST_CASE("GLB preserves hierarchy/nonuniform scale and repairs mirrored winding")
{
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "triangle.glb";
    write_glb(path, triangle_json);
    const auto model = RelayCore::tools::import_glb(path);
    REQUIRE(model.parts.size() == 2);
    CHECK(model.meshes.size() == 2);
    CHECK(model.materials.size() == 1);
    CHECK(model.parts[0].material == model.parts[1].material);
    for (const auto& part : model.parts)
        CHECK(glm::determinant(glm::mat3{part.transform}) > 0);
    auto position = glm::vec3{model.parts[0].transform * glm::vec4{model.meshes[0].geometry.vertices[1].position, 1}};
    CHECK(position.x == doctest::Approx(3));
    CHECK(position.y == doctest::Approx(4));
    const auto& mirrored = model.meshes[model.parts[1].mesh].geometry;
    CHECK(mirrored.indices == std::vector<std::uint32_t>{0, 2, 1});
    position = glm::vec3{model.parts[1].transform * glm::vec4{mirrored.vertices[1].position, 1}};
    CHECK(position.x == doctest::Approx(3));
    CHECK(position.y == doctest::Approx(-1));
    CHECK(position.z == doctest::Approx(2));
    CHECK(model.materials[0].base_color.pixels == std::vector<std::uint8_t>{137, 188, 255, 255});
}

TEST_CASE("GLB rejects unsupported and damaged input with its source path")
{
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "bad.glb";
    for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
             {"\"count\":3", "\"count\":1000000000"},
             {"\"byteLength\":36", "\"byteLength\":4"},
             {"\"NORMAL\":1", "\"MISSING\":1"},
             {"\"children\":[1,2]", "\"children\":[0]"},
             {"\"material\":0", "\"material\":999"},
             {"\"pbrMetallicRoughness\"", "\"alphaMode\":\"BLEND\",\"pbrMetallicRoughness\""}
         })
    {
        std::string json = triangle_json;
        json.replace(json.find(from), from.size(), to);
        write_glb(path, json);
        CHECK_THROWS_WITH_AS(
            static_cast<void>(RelayCore::tools::import_glb(path)),
            doctest::Contains("bad.glb"),
            std::runtime_error
        );
    }
    write_glb(path, triangle_json, true);
    CHECK_THROWS_WITH_AS(
        static_cast<void>(RelayCore::tools::import_glb(path)),
        doctest::Contains("finite"),
        std::runtime_error
    );
    std::ofstream(path) << "broken";
    CHECK_THROWS_WITH_AS(
        static_cast<void>(RelayCore::tools::import_glb(path)),
        doctest::Contains("bad.glb"),
        std::runtime_error
    );
}

TEST_CASE("Blender cylinder GLB recompiles to the staged native product")
{
    const auto model = RelayCore::tools::import_glb(RELAYCORE_MODEL_SOURCE);
    REQUIRE(model.parts.size() == 2);
    REQUIRE(model.materials.size() == 2);
    CHECK(model.materials[0].base_color.width == 1);
    CHECK(model.materials[1].base_color.width > 1);
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "cylinder.rcmodel";
    RelayCore::render::write_model_asset(path, model);
    std::ifstream generated(path, std::ios::binary),
        shipped(RelayCore::core::asset_path("models/service_cylinder.rcmodel"), std::ios::binary);
    CHECK(
        std::string(std::istreambuf_iterator<char>(generated), {})
        == std::string(std::istreambuf_iterator<char>(shipped), {})
    );
}

TEST_CASE("GLB normal map preserves linear bytes, scale and mirrored tangent handedness")
{
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "normal.glb";
    std::string json = triangle_json;
    const auto replace = [&](const std::string& from, const std::string& to) {
        json.replace(json.find(from), from.size(), to);
    };
    replace("\"TEXCOORD_0\":2", "\"TEXCOORD_0\":2,\"TANGENT\":3");
    replace("\"pbrMetallicRoughness\"", "\"normalTexture\":{\"index\":0,\"scale\":0.5},\"pbrMetallicRoughness\"");
    replace("\"byteLength\":96", "\"byteLength\":" + std::to_string(144 + normal_png.size()));
    replace(
        "\"byteLength\":24}",
        "\"byteLength\":24},{\"buffer\":0,\"byteOffset\":96,\"byteLength\":48},{\"buffer\":0,\"byteOffset\":144,"
        "\"byteLength\":"
            + std::to_string(normal_png.size()) + "}"
    );
    replace(
        "\"type\":\"VEC2\"}",
        "\"type\":\"VEC2\"},{\"bufferView\":3,\"componentType\":5126,\"count\":3,\"type\":\"VEC4\"}"
    );
    json.insert(json.rfind('}'), R"(,"textures":[{"source":0}],"images":[{"bufferView":4,"mimeType":"image/png"}])");
    write_glb(path, json, false, true);
    const auto model = RelayCore::tools::import_glb(path);
    CHECK(model.materials[0].normal->pixels == std::vector<std::uint8_t>{128, 192, 240, 255});
    CHECK(model.materials[0].normal_scale == .5F);
    CHECK(model.meshes[0].has_tangents);
    CHECK(model.meshes[0].geometry.vertices[0].tangent == glm::vec4{1, 0, 0, 1});
    CHECK(model.meshes[model.parts[1].mesh].geometry.vertices[0].tangent == glm::vec4{-1, 0, 0, -1});
    for (const auto& [from, to, diagnostic] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {",\"TANGENT\":3", "", "TANGENT"},
             {"\"index\":0,\"scale\":0.5", "\"index\":0,\"texCoord\":1", "TEXCOORD_0"},
             {"\"normalTexture\"", "\"occlusionTexture\"", "occlusion"}
         })
    {
        auto bad = json;
        bad.replace(bad.find(from), from.size(), to);
        write_glb(path, bad, false, true);
        CHECK_THROWS_WITH_AS(
            static_cast<void>(RelayCore::tools::import_glb(path)),
            doctest::Contains(diagnostic),
            std::runtime_error
        );
    }
}

TEST_CASE("Blender cabinet metallic-roughness material recompiles to the staged v3 product")
{
    const auto source = std::filesystem::path{RELAYCORE_MODEL_SOURCE}.parent_path() / "power_cabinet.glb";
    const auto model = RelayCore::tools::import_glb(source, true);
    const auto material = std::find_if(model.materials.begin(), model.materials.end(), [](const auto& value) {
        return value.normal.has_value();
    });
    REQUIRE(material != model.materials.end());
    CHECK(material->normal->width == 256);
    CHECK(material->normal->height == 256);
    REQUIRE(material->metallic_roughness);
    CHECK(material->metallic_roughness->roughness == doctest::Approx(.68));
    CHECK(material->metallic_roughness->metallic == 0);
    CHECK(std::any_of(model.materials.begin(), model.materials.end(), [](const auto& m) {
        return m.metallic_roughness->metallic == 1;
    }));
    for (const auto& part : model.parts)
    {
        if (model.materials[part.material].normal)
            CHECK(model.meshes[part.mesh].has_tangents);
        // Bare metal belongs to compact hardware, not the painted panels/frame/plinth.
        if (model.materials[part.material].metallic_roughness->metallic == 1)
        {
            glm::vec3 mins{std::numeric_limits<float>::max()}, maxs{-mins};
            for (const auto& vertex : model.meshes[part.mesh].geometry.vertices)
            {
                const auto position = glm::vec3{part.transform * glm::vec4{vertex.position, 1}};
                mins = glm::min(mins, position);
                maxs = glm::max(maxs, position);
            }
            const auto size = maxs - mins;
            CHECK(size.x < .1F);
            CHECK(size.y < .3F);
            CHECK(size.z < .15F);
        }
    }
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "cabinet.rcmodel";
    RelayCore::render::write_model_asset(path, model);
    std::ifstream generated(path, std::ios::binary),
        shipped(RelayCore::core::asset_path("models/power_cabinet.rcmodel"), std::ios::binary);
    CHECK(
        std::string(std::istreambuf_iterator<char>(generated), {})
        == std::string(std::istreambuf_iterator<char>(shipped), {})
    );
    RelayCore::render::StaticModelCache cache;
    for (const auto encoding : {RelayCore::render::TextureEncoding::linear, RelayCore::render::TextureEncoding::srgb})
    {
        const auto runtime = cache.load(path, encoding);
        const auto& surface = runtime->materials[static_cast<std::size_t>(material - model.materials.begin())];
        CHECK(surface.base_color.encoding() == encoding);
        CHECK(surface.normal.encoding() == RelayCore::render::TextureEncoding::linear);
        CHECK(surface.normal.cpu_data()->pixels == material->normal->pixels);
        CHECK(surface.metallic_roughness == material->metallic_roughness);
    }
}

TEST_CASE("GLB metallic-roughness import is explicit and rejects unsupported maps")
{
    RelayCore::test::ScopedTempDirectory temporary;
    const auto path = temporary.path() / "material.glb";
    auto json = std::string{triangle_json};
    json.replace(json.find("\"metallicFactor\":0"), 18, "\"metallicFactor\":0.7,\"roughnessFactor\":0.25");
    write_glb(path, json);
    CHECK_FALSE(RelayCore::tools::import_glb(path).materials[0].metallic_roughness);
    const auto model = RelayCore::tools::import_glb(path, true);
    REQUIRE(model.materials[0].metallic_roughness);
    CHECK(model.materials[0].metallic_roughness->roughness == .25F);
    CHECK(model.materials[0].metallic_roughness->metallic == doctest::Approx(.7));
    json.insert(json.find("\"baseColorFactor\""), "\"metallicRoughnessTexture\":{\"index\":0},");
    json.insert(json.rfind('}'), R"(,"textures":[{"source":0}],"images":[{"uri":"missing.png"}])");
    write_glb(path, json);
    CHECK_THROWS_WITH_AS(
        static_cast<void>(RelayCore::tools::import_glb(path, true)),
        doctest::Contains("metallic-roughness maps"),
        std::runtime_error
    );
}
