#include "relaycore/render/mesh.h"

#include <array>
#include <cstddef>
#include <glad/gl.h>
#include <stdexcept>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        struct CubeVertex {
            float position_x;
            float position_y;
            float position_z;
            float color_r;
            float color_g;
            float color_b;
        };

        constexpr std::array<CubeVertex, 36> kCubeVertices{
            CubeVertex{-0.5F, -0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            CubeVertex{0.5F, -0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            CubeVertex{0.5F, 0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            CubeVertex{-0.5F, -0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            CubeVertex{0.5F, 0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            CubeVertex{-0.5F, 0.5F, 0.5F, 0.95F, 0.30F, 0.25F},

            CubeVertex{0.5F, -0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            CubeVertex{-0.5F, -0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            CubeVertex{-0.5F, 0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            CubeVertex{0.5F, -0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            CubeVertex{-0.5F, 0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            CubeVertex{0.5F, 0.5F, -0.5F, 0.25F, 0.75F, 0.35F},

            CubeVertex{-0.5F, -0.5F, -0.5F, 0.20F, 0.55F, 0.95F},
            CubeVertex{-0.5F, -0.5F, 0.5F, 0.20F, 0.55F, 0.95F},
            CubeVertex{-0.5F, 0.5F, 0.5F, 0.20F, 0.55F, 0.95F},
            CubeVertex{-0.5F, -0.5F, -0.5F, 0.20F, 0.55F, 0.95F},
            CubeVertex{-0.5F, 0.5F, 0.5F, 0.20F, 0.55F, 0.95F},
            CubeVertex{-0.5F, 0.5F, -0.5F, 0.20F, 0.55F, 0.95F},

            CubeVertex{0.5F, -0.5F, 0.5F, 0.98F, 0.82F, 0.30F},
            CubeVertex{0.5F, -0.5F, -0.5F, 0.98F, 0.82F, 0.30F},
            CubeVertex{0.5F, 0.5F, -0.5F, 0.98F, 0.82F, 0.30F},
            CubeVertex{0.5F, -0.5F, 0.5F, 0.98F, 0.82F, 0.30F},
            CubeVertex{0.5F, 0.5F, -0.5F, 0.98F, 0.82F, 0.30F},
            CubeVertex{0.5F, 0.5F, 0.5F, 0.98F, 0.82F, 0.30F},

            CubeVertex{-0.5F, 0.5F, 0.5F, 0.80F, 0.35F, 0.90F},
            CubeVertex{0.5F, 0.5F, 0.5F, 0.80F, 0.35F, 0.90F},
            CubeVertex{0.5F, 0.5F, -0.5F, 0.80F, 0.35F, 0.90F},
            CubeVertex{-0.5F, 0.5F, 0.5F, 0.80F, 0.35F, 0.90F},
            CubeVertex{0.5F, 0.5F, -0.5F, 0.80F, 0.35F, 0.90F},
            CubeVertex{-0.5F, 0.5F, -0.5F, 0.80F, 0.35F, 0.90F},

            CubeVertex{-0.5F, -0.5F, -0.5F, 0.30F, 0.85F, 0.90F},
            CubeVertex{0.5F, -0.5F, -0.5F, 0.30F, 0.85F, 0.90F},
            CubeVertex{0.5F, -0.5F, 0.5F, 0.30F, 0.85F, 0.90F},
            CubeVertex{-0.5F, -0.5F, -0.5F, 0.30F, 0.85F, 0.90F},
            CubeVertex{0.5F, -0.5F, 0.5F, 0.30F, 0.85F, 0.90F},
            CubeVertex{-0.5F, -0.5F, 0.5F, 0.30F, 0.85F, 0.90F},
        };

        void configure_color_vertex_layout()
        {
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
        }
    } // namespace

    StaticColorMesh::~StaticColorMesh() noexcept { reset(); }

    StaticColorMesh::StaticColorMesh(StaticColorMesh&& other) noexcept :
        vertex_array_id_(std::exchange(other.vertex_array_id_, 0)),
        vertex_buffer_id_(std::exchange(other.vertex_buffer_id_, 0)),
        index_buffer_id_(std::exchange(other.index_buffer_id_, 0)),
        index_count_(std::exchange(other.index_count_, 0)) {}

    auto StaticColorMesh::operator=(StaticColorMesh&& other) noexcept -> StaticColorMesh&
    {
        if (this != &other)
        {
            reset();
            vertex_array_id_  = std::exchange(other.vertex_array_id_, 0);
            vertex_buffer_id_ = std::exchange(other.vertex_buffer_id_, 0);
            index_buffer_id_  = std::exchange(other.index_buffer_id_, 0);
            index_count_      = std::exchange(other.index_count_, 0);
        }

        return *this;
    }

    void StaticColorMesh::initialize(const CpuColorMeshData& mesh_data)
    {
        if (mesh_data.vertices.empty() || mesh_data.indices.empty())
        {
            throw std::runtime_error("StaticColorMesh requires non-empty vertex and index buffers");
        }

        reset();
        index_count_ = static_cast<GLsizei>(mesh_data.indices.size());

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
        glBindVertexArray(vertex_array_id_);
        glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    auto StaticColorMesh::index_count() const noexcept -> GLsizei { return index_count_; }

    void StaticColorMesh::reset() noexcept
    {
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

    ColorCubeMesh::~ColorCubeMesh() noexcept { reset(); }

    ColorCubeMesh::ColorCubeMesh(ColorCubeMesh&& other) noexcept :
        vertex_array_id_(std::exchange(other.vertex_array_id_, 0)),
        vertex_buffer_id_(std::exchange(other.vertex_buffer_id_, 0)) {}

    auto ColorCubeMesh::operator=(ColorCubeMesh&& other) noexcept -> ColorCubeMesh&
    {
        if (this != &other)
        {
            reset();
            vertex_array_id_  = std::exchange(other.vertex_array_id_, 0);
            vertex_buffer_id_ = std::exchange(other.vertex_buffer_id_, 0);
        }

        return *this;
    }

    void ColorCubeMesh::initialize()
    {
        reset();

        glGenVertexArrays(1, &vertex_array_id_);
        glGenBuffers(1, &vertex_buffer_id_);

        glBindVertexArray(vertex_array_id_);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(sizeof(kCubeVertices)),
            kCubeVertices.data(),
            GL_STATIC_DRAW
        );

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(CubeVertex)),
            reinterpret_cast<const void*>(offsetof(CubeVertex, position_x))
        );

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(CubeVertex)),
            reinterpret_cast<const void*>(offsetof(CubeVertex, color_r))
        );

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void ColorCubeMesh::draw() const
    {
        glBindVertexArray(vertex_array_id_);
        glDrawArrays(GL_TRIANGLES, 0, kVertexCount);
        glBindVertexArray(0);
    }

    void ColorCubeMesh::reset() noexcept
    {
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
    }
} // namespace RelayCore::render
