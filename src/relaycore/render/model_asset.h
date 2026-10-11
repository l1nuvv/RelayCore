#pragma once

#include "relaycore/render/material.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/texture2d.h"

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <optional>

namespace RelayCore::render
{
    struct MeshAsset {
        CpuColorMeshData geometry;
        bool has_tangents = false;
    };

    struct ModelMaterial {
        CpuRgbaTextureData base_color;
        std::optional<CpuRgbaTextureData> normal;
        float normal_scale = 1;
        std::optional<MetallicRoughness> metallic_roughness;
        std::optional<CpuRgbaTextureData> metallic_roughness_map;
        std::optional<CpuRgbaTextureData> occlusion;
        float occlusion_strength = 1;
        std::optional<CpuRgbaTextureData> emissive;
        glm::vec3 emissive_factor{0};
    };

    struct ModelPart {
        std::uint32_t mesh = 0;
        std::uint32_t material = 0;
        glm::mat4 transform{1};
    };

    // Versioned native product: metres, +Y up, right-handed, top-left texture/UV origin.
    struct ModelAssetData {
        std::vector<MeshAsset> meshes;
        std::vector<ModelMaterial> materials;
        std::vector<ModelPart> parts;
    };

    void validate_model_asset(const ModelAssetData& model);
    void write_model_asset(const std::filesystem::path& path, const ModelAssetData& model);
    [[nodiscard]] auto read_model_asset(const std::filesystem::path& path) -> ModelAssetData;

    struct ModelSurface {
        Texture2D base_color;
        Texture2D normal;
        float normal_scale = 1;
        std::optional<MetallicRoughness> metallic_roughness;
        Texture2D metallic_roughness_map;
        Texture2D occlusion;
        float occlusion_strength = 1;
        Texture2D emissive;
        glm::vec3 emissive_factor{0};
    };

    struct ModelAsset {
        std::vector<StaticColorMesh> meshes;
        std::vector<ModelSurface> materials;
        std::vector<ModelPart> parts;
        glm::vec3 mins{}, maxs{};
    };
} // namespace RelayCore::render
