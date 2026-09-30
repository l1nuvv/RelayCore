#pragma once

#include "relaycore/render/resource_upload.h"

#include <cstdint>
#include <glad/gl.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

namespace RelayCore::render
{
    struct ColorVertex {
        glm::vec3 position{};
        glm::vec3 color{};
        glm::vec3 normal{};
        float material_kind = 0.0F;
        glm::vec2 uv{};
    };

    struct CpuColorMeshData {
        std::vector<ColorVertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    [[nodiscard]] auto make_color_box_mesh_data() -> CpuColorMeshData;

    [[nodiscard]] auto make_color_double_sided_quad_mesh_data() -> CpuColorMeshData;

    class StaticColorMesh {
    public:
        StaticColorMesh() = default;

        ~StaticColorMesh() noexcept;

        StaticColorMesh(const StaticColorMesh&) = delete;

        auto operator=(const StaticColorMesh&) -> StaticColorMesh& = delete;

        StaticColorMesh(StaticColorMesh&& other) noexcept;

        auto operator=(StaticColorMesh&& other) noexcept -> StaticColorMesh&;

        void initialize(const CpuColorMeshData& mesh_data, ResourceUpload upload = ResourceUpload::opengl);

        // CPU-mode resources retain immutable data; replacing it changes the renderer cache identity.
        [[nodiscard]] auto cpu_data() const noexcept -> const std::shared_ptr<const CpuColorMeshData>&
        {
            return cpu_data_;
        }

        void draw() const;

        [[nodiscard]] auto index_count() const noexcept -> GLsizei;

        [[nodiscard]] auto bounds_min() const noexcept -> glm::vec3
        {
            return bounds_min_;
        }
        [[nodiscard]] auto bounds_max() const noexcept -> glm::vec3
        {
            return bounds_max_;
        }

    private:
        void reset() noexcept;

        unsigned int vertex_array_id_ = 0;
        unsigned int vertex_buffer_id_ = 0;
        unsigned int index_buffer_id_ = 0;
        GLsizei index_count_ = 0;
        glm::vec3 bounds_min_{};
        glm::vec3 bounds_max_{};
        std::shared_ptr<const CpuColorMeshData> cpu_data_;
    };

} // namespace RelayCore::render
