/**
 * @file src/relaycore/render/mesh.h
 * @brief Static debug mesh definitions used by the renderer.
 */

#pragma once

#include <glad/gl.h>

namespace RelayCore::render
{
    /**
     * @brief GPU mesh for a unit-sized color cube.
     */
    class ColorCubeMesh {
    public:
        /**
         * @brief Creates an empty mesh handle wrapper.
         */
        ColorCubeMesh() = default;

        /**
         * @brief Releases the owned GPU objects.
         */
        ~ColorCubeMesh() noexcept;

        ColorCubeMesh(const ColorCubeMesh&) = delete; ///< ColorCubeMesh is not copyable.

        /**
         * @brief Disables copy assignment.
         * @return Reference to this object.
         */
        auto operator=(const ColorCubeMesh&) -> ColorCubeMesh& = delete;

        /**
         * @brief Moves GPU object ownership into a new mesh wrapper.
         * @param other Source mesh wrapper.
         */
        ColorCubeMesh(ColorCubeMesh&& other) noexcept;

        /**
         * @brief Moves GPU object ownership into this mesh wrapper.
         * @param other Source mesh wrapper.
         * @return Reference to this object.
         */
        auto operator=(ColorCubeMesh&& other) noexcept -> ColorCubeMesh&;

        /**
         * @brief Uploads cube vertex data and configures VAO state.
         */
        void initialize();

        /**
         * @brief Draws the cube mesh.
         */
        void draw() const;

    private:
        static constexpr GLsizei kVertexCount = 36; ///< Number of vertices submitted for the cube.

        /**
         * @brief Releases owned VAO and VBO objects.
         */
        void reset() noexcept;

        unsigned int vertex_array_id_  = 0; ///< Vertex-array object identifier.
        unsigned int vertex_buffer_id_ = 0; ///< Vertex-buffer object identifier.
    };
} // namespace RelayCore::render
