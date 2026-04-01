#pragma once

#include <glad/gl.h>

namespace RelayCore::render
{
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
