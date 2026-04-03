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
    };

    struct CpuColorMeshData {
        std::vector<ColorVertex>   vertices;
        std::vector<std::uint32_t> indices;
    };

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

        unsigned int vertex_array_id_  = 0;
        unsigned int vertex_buffer_id_ = 0;
        unsigned int index_buffer_id_  = 0;
        GLsizei      index_count_      = 0;
    };

    class ColorCubeMesh {
    public:
        ColorCubeMesh() = default;

        ~ColorCubeMesh() noexcept;

        ColorCubeMesh(const ColorCubeMesh&) = delete;

        auto operator=(const ColorCubeMesh&) -> ColorCubeMesh& = delete;

        ColorCubeMesh(ColorCubeMesh&& other) noexcept;

        auto operator=(ColorCubeMesh&& other) noexcept -> ColorCubeMesh&;

        void initialize();

        void draw() const;

    private:
        static constexpr GLsizei kVertexCount = 36;

        void reset() noexcept;

        unsigned int vertex_array_id_  = 0;
        unsigned int vertex_buffer_id_ = 0;
    };
} // namespace RelayCore::render
