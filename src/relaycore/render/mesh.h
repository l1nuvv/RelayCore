#pragma once

#include <cstdint>
#include <glad/gl.h>
#include <glm/vec3.hpp>
#include <vector>

namespace RelayCore::render
{
    struct ColorVertex {
        glm::vec3 position{};
        glm::vec3 color{};
        glm::vec3 normal{};
        float material_kind = 0.0F;
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

        void initialize(const CpuColorMeshData& mesh_data);

        void draw() const;

        [[nodiscard]] auto index_count() const noexcept -> GLsizei;

    private:
        void reset() noexcept;

        unsigned int vertex_array_id_ = 0;
        unsigned int vertex_buffer_id_ = 0;
        unsigned int index_buffer_id_ = 0;
        GLsizei index_count_ = 0;
    };

} // namespace RelayCore::render
