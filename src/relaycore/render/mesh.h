#pragma once

#include "relaycore/render/meshlets.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
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
        // glTF tangent basis: B = cross(N, tangent.xyz) * tangent.w.
        glm::vec4 tangent{0, 0, 0, 1};
    };

    struct CpuColorMeshData {
        std::vector<ColorVertex> vertices;
        std::vector<std::uint32_t> indices;
        MeshletData meshlets;
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

        // Resources retain immutable data; replacing it changes the renderer cache identity.
        [[nodiscard]] auto cpu_data() const noexcept -> const std::shared_ptr<const CpuColorMeshData>&
        {
            return cpu_data_;
        }

        [[nodiscard]] auto index_count() const noexcept -> std::int32_t;

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

        std::int32_t index_count_ = 0;
        glm::vec3 bounds_min_{};
        glm::vec3 bounds_max_{};
        std::shared_ptr<const CpuColorMeshData> cpu_data_;
    };

} // namespace RelayCore::render
