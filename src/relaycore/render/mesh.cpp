#include "relaycore/render/mesh.h"

#include "relaycore/core/profiling.h"

#include <cstddef>
#include <glad/gl.h>
#include <glm/common.hpp>
#include <stdexcept>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        void configure_color_vertex_layout()
        {
            RELAYCORE_PROFILE_FUNCTION();
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(
                0,
                3,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(sizeof(ColorVertex)),
                reinterpret_cast<const void*>(offsetof(ColorVertex, position))
            );

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(
                1,
                3,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(sizeof(ColorVertex)),
                reinterpret_cast<const void*>(offsetof(ColorVertex, color))
            );

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(
                2,
                3,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(sizeof(ColorVertex)),
                reinterpret_cast<const void*>(offsetof(ColorVertex, normal))
            );

            glEnableVertexAttribArray(3);
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(
                4,
                2,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(sizeof(ColorVertex)),
                reinterpret_cast<const void*>(offsetof(ColorVertex, uv))
            );
            glVertexAttribPointer(
                3,
                1,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(sizeof(ColorVertex)),
                reinterpret_cast<const void*>(offsetof(ColorVertex, material_kind))
            );
        }
    } // namespace

    auto make_color_box_mesh_data() -> CpuColorMeshData
    {
        RELAYCORE_PROFILE_FUNCTION();
        CpuColorMeshData mesh_data{};
        mesh_data.vertices = {
            {{-0.5F, -0.5F, 0.5F}, {0.42F, 0.54F, 0.47F}, {0.0F, 0.0F, 1.0F}, 0.0F},
            {{0.5F, -0.5F, 0.5F}, {0.42F, 0.54F, 0.47F}, {0.0F, 0.0F, 1.0F}, 0.0F},
            {{0.5F, 0.5F, 0.5F}, {0.42F, 0.54F, 0.47F}, {0.0F, 0.0F, 1.0F}, 0.0F},
            {{-0.5F, 0.5F, 0.5F}, {0.42F, 0.54F, 0.47F}, {0.0F, 0.0F, 1.0F}, 0.0F},

            {{0.5F, -0.5F, -0.5F}, {0.32F, 0.41F, 0.36F}, {0.0F, 0.0F, -1.0F}, 0.0F},
            {{-0.5F, -0.5F, -0.5F}, {0.32F, 0.41F, 0.36F}, {0.0F, 0.0F, -1.0F}, 0.0F},
            {{-0.5F, 0.5F, -0.5F}, {0.32F, 0.41F, 0.36F}, {0.0F, 0.0F, -1.0F}, 0.0F},
            {{0.5F, 0.5F, -0.5F}, {0.32F, 0.41F, 0.36F}, {0.0F, 0.0F, -1.0F}, 0.0F},

            {{-0.5F, -0.5F, -0.5F}, {0.29F, 0.36F, 0.33F}, {-1.0F, 0.0F, 0.0F}, 0.0F},
            {{-0.5F, -0.5F, 0.5F}, {0.29F, 0.36F, 0.33F}, {-1.0F, 0.0F, 0.0F}, 0.0F},
            {{-0.5F, 0.5F, 0.5F}, {0.29F, 0.36F, 0.33F}, {-1.0F, 0.0F, 0.0F}, 0.0F},
            {{-0.5F, 0.5F, -0.5F}, {0.29F, 0.36F, 0.33F}, {-1.0F, 0.0F, 0.0F}, 0.0F},

            {{0.5F, -0.5F, 0.5F}, {0.50F, 0.61F, 0.54F}, {1.0F, 0.0F, 0.0F}, 0.0F},
            {{0.5F, -0.5F, -0.5F}, {0.50F, 0.61F, 0.54F}, {1.0F, 0.0F, 0.0F}, 0.0F},
            {{0.5F, 0.5F, -0.5F}, {0.50F, 0.61F, 0.54F}, {1.0F, 0.0F, 0.0F}, 0.0F},
            {{0.5F, 0.5F, 0.5F}, {0.50F, 0.61F, 0.54F}, {1.0F, 0.0F, 0.0F}, 0.0F},

            {{-0.5F, 0.5F, 0.5F}, {0.54F, 0.66F, 0.58F}, {0.0F, 1.0F, 0.0F}, 0.0F},
            {{0.5F, 0.5F, 0.5F}, {0.54F, 0.66F, 0.58F}, {0.0F, 1.0F, 0.0F}, 0.0F},
            {{0.5F, 0.5F, -0.5F}, {0.54F, 0.66F, 0.58F}, {0.0F, 1.0F, 0.0F}, 0.0F},
            {{-0.5F, 0.5F, -0.5F}, {0.54F, 0.66F, 0.58F}, {0.0F, 1.0F, 0.0F}, 0.0F},

            {{-0.5F, -0.5F, -0.5F}, {0.22F, 0.29F, 0.26F}, {0.0F, -1.0F, 0.0F}, 0.0F},
            {{0.5F, -0.5F, -0.5F}, {0.22F, 0.29F, 0.26F}, {0.0F, -1.0F, 0.0F}, 0.0F},
            {{0.5F, -0.5F, 0.5F}, {0.22F, 0.29F, 0.26F}, {0.0F, -1.0F, 0.0F}, 0.0F},
            {{-0.5F, -0.5F, 0.5F}, {0.22F, 0.29F, 0.26F}, {0.0F, -1.0F, 0.0F}, 0.0F},
        };
        mesh_data.indices = {
            0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
            12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
        };
        return mesh_data;
    }

    auto make_color_double_sided_quad_mesh_data() -> CpuColorMeshData
    {
        RELAYCORE_PROFILE_FUNCTION();
        CpuColorMeshData mesh_data{};
        mesh_data.vertices = {
            {{-0.5F, -0.5F, 0.0F}, {0.42F, 0.54F, 0.47F}, {0.0F, 0.0F, 1.0F}, 0.0F},
            {{0.5F, -0.5F, 0.0F}, {0.42F, 0.54F, 0.47F}, {0.0F, 0.0F, 1.0F}, 0.0F},
            {{0.5F, 0.5F, 0.0F}, {0.42F, 0.54F, 0.47F}, {0.0F, 0.0F, 1.0F}, 0.0F},
            {{-0.5F, 0.5F, 0.0F}, {0.42F, 0.54F, 0.47F}, {0.0F, 0.0F, 1.0F}, 0.0F},
            {{0.5F, -0.5F, 0.0F}, {0.32F, 0.41F, 0.36F}, {0.0F, 0.0F, -1.0F}, 0.0F},
            {{-0.5F, -0.5F, 0.0F}, {0.32F, 0.41F, 0.36F}, {0.0F, 0.0F, -1.0F}, 0.0F},
            {{-0.5F, 0.5F, 0.0F}, {0.32F, 0.41F, 0.36F}, {0.0F, 0.0F, -1.0F}, 0.0F},
            {{0.5F, 0.5F, 0.0F}, {0.32F, 0.41F, 0.36F}, {0.0F, 0.0F, -1.0F}, 0.0F},
        };
        mesh_data.indices = {
            0,
            1,
            2,
            0,
            2,
            3,
            4,
            5,
            6,
            4,
            6,
            7,
        };
        return mesh_data;
    }

    StaticColorMesh::~StaticColorMesh() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        reset();
    }

    StaticColorMesh::StaticColorMesh(StaticColorMesh&& other) noexcept :
        vertex_array_id_(std::exchange(other.vertex_array_id_, 0)),
        vertex_buffer_id_(std::exchange(other.vertex_buffer_id_, 0)),
        index_buffer_id_(std::exchange(other.index_buffer_id_, 0)), index_count_(std::exchange(other.index_count_, 0))
    {
        RELAYCORE_PROFILE_FUNCTION();
        bounds_min_ = other.bounds_min_;
        bounds_max_ = other.bounds_max_;
    }

    auto StaticColorMesh::operator=(StaticColorMesh&& other) noexcept -> StaticColorMesh&
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (this != &other)
        {
            reset();
            vertex_array_id_ = std::exchange(other.vertex_array_id_, 0);
            vertex_buffer_id_ = std::exchange(other.vertex_buffer_id_, 0);
            index_buffer_id_ = std::exchange(other.index_buffer_id_, 0);
            index_count_ = std::exchange(other.index_count_, 0);
            bounds_min_ = other.bounds_min_;
            bounds_max_ = other.bounds_max_;
        }

        return *this;
    }

    void StaticColorMesh::initialize(const CpuColorMeshData& mesh_data)
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (mesh_data.vertices.empty() || mesh_data.indices.empty())
        {
            throw std::runtime_error("StaticColorMesh requires non-empty vertex and index buffers");
        }

        reset();
        index_count_ = static_cast<GLsizei>(mesh_data.indices.size());
        bounds_min_ = mesh_data.vertices.front().position;
        bounds_max_ = bounds_min_;
        for (const ColorVertex& vertex : mesh_data.vertices)
        {
            bounds_min_ = glm::min(bounds_min_, vertex.position);
            bounds_max_ = glm::max(bounds_max_, vertex.position);
        }

        glGenVertexArrays(1, &vertex_array_id_);
        glGenBuffers(1, &vertex_buffer_id_);
        glGenBuffers(1, &index_buffer_id_);

        glBindVertexArray(vertex_array_id_);

        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(mesh_data.vertices.size() * sizeof(ColorVertex)),
            mesh_data.vertices.data(),
            GL_STATIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id_);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(mesh_data.indices.size() * sizeof(std::uint32_t)),
            mesh_data.indices.data(),
            GL_STATIC_DRAW
        );

        configure_color_vertex_layout();

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void StaticColorMesh::draw() const
    {
        RELAYCORE_PROFILE_ZONE_N("StaticColorMesh::draw");
        glBindVertexArray(vertex_array_id_);
        glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    auto StaticColorMesh::index_count() const noexcept -> GLsizei
    {
        RELAYCORE_PROFILE_FUNCTION();
        return index_count_;
    }

    void StaticColorMesh::reset() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (index_buffer_id_ != 0)
        {
            glDeleteBuffers(1, &index_buffer_id_);
            index_buffer_id_ = 0;
        }

        if (vertex_buffer_id_ != 0)
        {
            glDeleteBuffers(1, &vertex_buffer_id_);
            vertex_buffer_id_ = 0;
        }

        if (vertex_array_id_ != 0)
        {
            glDeleteVertexArrays(1, &vertex_array_id_);
            vertex_array_id_ = 0;
        }

        index_count_ = 0;
    }
} // namespace RelayCore::render
