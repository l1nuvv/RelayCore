#pragma once

#include "relaycore/render/mesh.h"
#include "relaycore/render/texture2d.h"

#include <filesystem>
#include <memory>
#include <unordered_map>

namespace RelayCore::render
{
    // Triangulated OBJ with explicit v/vt/vn; the scene selects its single diffuse material.
    [[nodiscard]] auto load_obj_mesh(const std::filesystem::path& path) -> CpuColorMeshData;

    struct StaticModel {
        StaticColorMesh mesh;
        Texture2D texture;
        glm::vec3 mins{};
        glm::vec3 maxs{};
    };

    // Scenes retain shared immutable resources. OpenGL mode requires a live context; CPU mode does not.
    class StaticModelCache {
    public:
        explicit StaticModelCache(ResourceUpload upload = ResourceUpload::opengl) : upload_(upload) {}
        [[nodiscard]] auto upload_mode() const noexcept -> ResourceUpload
        {
            return upload_;
        }
        [[nodiscard]] auto
        load(const std::filesystem::path& mesh, const std::filesystem::path& texture, TextureEncoding encoding)
            -> std::shared_ptr<const StaticModel>;

    private:
        struct Entry {
            std::filesystem::file_time_type mesh_time{}, texture_time{};
            std::uintmax_t mesh_size = 0, texture_size = 0;
            std::shared_ptr<const StaticModel> resource;
        };
        std::unordered_map<std::string, Entry> entries_;
        ResourceUpload upload_;
    };
} // namespace RelayCore::render
