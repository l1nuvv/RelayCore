#include "relaycore/render/mesh.hpp"

#include <array>
#include <cstddef>
#include <glad/gl.h>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        struct Vertex {
            float position_x;
            float position_y;
            float position_z;
            float color_r;
            float color_g;
            float color_b;
        };

        constexpr std::array<Vertex, 36> kCubeVertices{
            // front
            Vertex{-0.5F, -0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            Vertex{0.5F, -0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            Vertex{0.5F, 0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            Vertex{-0.5F, -0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            Vertex{0.5F, 0.5F, 0.5F, 0.95F, 0.30F, 0.25F},
            Vertex{-0.5F, 0.5F, 0.5F, 0.95F, 0.30F, 0.25F},

            // back
            Vertex{0.5F, -0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            Vertex{-0.5F, -0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            Vertex{-0.5F, 0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            Vertex{0.5F, -0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            Vertex{-0.5F, 0.5F, -0.5F, 0.25F, 0.75F, 0.35F},
            Vertex{0.5F, 0.5F, -0.5F, 0.25F, 0.75F, 0.35F},

            // left
            Vertex{-0.5F, -0.5F, -0.5F, 0.20F, 0.55F, 0.95F},
            Vertex{-0.5F, -0.5F, 0.5F, 0.20F, 0.55F, 0.95F},
            Vertex{-0.5F, 0.5F, 0.5F, 0.20F, 0.55F, 0.95F},
            Vertex{-0.5F, -0.5F, -0.5F, 0.20F, 0.55F, 0.95F},
            Vertex{-0.5F, 0.5F, 0.5F, 0.20F, 0.55F, 0.95F},
            Vertex{-0.5F, 0.5F, -0.5F, 0.20F, 0.55F, 0.95F},

            // right
            Vertex{0.5F, -0.5F, 0.5F, 0.98F, 0.82F, 0.30F},
            Vertex{0.5F, -0.5F, -0.5F, 0.98F, 0.82F, 0.30F},
            Vertex{0.5F, 0.5F, -0.5F, 0.98F, 0.82F, 0.30F},
            Vertex{0.5F, -0.5F, 0.5F, 0.98F, 0.82F, 0.30F},
            Vertex{0.5F, 0.5F, -0.5F, 0.98F, 0.82F, 0.30F},
            Vertex{0.5F, 0.5F, 0.5F, 0.98F, 0.82F, 0.30F},

            // top
            Vertex{-0.5F, 0.5F, 0.5F, 0.80F, 0.35F, 0.90F},
            Vertex{0.5F, 0.5F, 0.5F, 0.80F, 0.35F, 0.90F},
            Vertex{0.5F, 0.5F, -0.5F, 0.80F, 0.35F, 0.90F},
            Vertex{-0.5F, 0.5F, 0.5F, 0.80F, 0.35F, 0.90F},
            Vertex{0.5F, 0.5F, -0.5F, 0.80F, 0.35F, 0.90F},
            Vertex{-0.5F, 0.5F, -0.5F, 0.80F, 0.35F, 0.90F},

            // bottom
            Vertex{-0.5F, -0.5F, -0.5F, 0.30F, 0.85F, 0.90F},
            Vertex{0.5F, -0.5F, -0.5F, 0.30F, 0.85F, 0.90F},
            Vertex{0.5F, -0.5F, 0.5F, 0.30F, 0.85F, 0.90F},
            Vertex{-0.5F, -0.5F, -0.5F, 0.30F, 0.85F, 0.90F},
            Vertex{0.5F, -0.5F, 0.5F, 0.30F, 0.85F, 0.90F},
            Vertex{-0.5F, -0.5F, 0.5F, 0.30F, 0.85F, 0.90F},
        };
    } // namespace

    ColorCubeMesh::~ColorCubeMesh() noexcept { reset(); }

    ColorCubeMesh::ColorCubeMesh(ColorCubeMesh&& other) noexcept :
        vertex_array_id_(std::exchange(other.vertex_array_id_, 0)),
        vertex_buffer_id_(std::exchange(other.vertex_buffer_id_, 0))
    {}

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
            GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(kCubeVertices)), kCubeVertices.data(), GL_STATIC_DRAW
        );

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(Vertex)),
            reinterpret_cast<const void*>(offsetof(Vertex, position_x))
        );

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(Vertex)),
            reinterpret_cast<const void*>(offsetof(Vertex, color_r))
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
