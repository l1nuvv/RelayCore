#include "relaycore/render/mesh.h"

#include "relaycore/core/profiling.h"

#include <cstddef>
#include <glm/common.hpp>
#include <limits>
#include <stdexcept>
#include <utility>

namespace RelayCore::render
{
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
        index_count_(std::exchange(other.index_count_, 0))
    {
        RELAYCORE_PROFILE_FUNCTION();
        bounds_min_ = other.bounds_min_;
        bounds_max_ = other.bounds_max_;
        cpu_data_ = std::move(other.cpu_data_);
    }

    auto StaticColorMesh::operator=(StaticColorMesh&& other) noexcept -> StaticColorMesh&
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (this != &other)
        {
            reset();
            index_count_ = std::exchange(other.index_count_, 0);
            bounds_min_ = other.bounds_min_;
            bounds_max_ = other.bounds_max_;
            cpu_data_ = std::move(other.cpu_data_);
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

        if (mesh_data.indices.size() > static_cast<size_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::invalid_argument("StaticColorMesh index count exceeds draw limits");
        for (const auto index : mesh_data.indices)
            if (index >= mesh_data.vertices.size())
                throw std::invalid_argument("StaticColorMesh index is outside its vertex buffer");

        if (!mesh_data.meshlets.clusters.empty())
            validate_meshlets(mesh_data, mesh_data.meshlets);
        reset();
        cpu_data_ = std::make_shared<CpuColorMeshData>(mesh_data);
        index_count_ = static_cast<std::int32_t>(mesh_data.indices.size());
        bounds_min_ = mesh_data.vertices.front().position;
        bounds_max_ = bounds_min_;
        for (const ColorVertex& vertex : mesh_data.vertices)
        {
            bounds_min_ = glm::min(bounds_min_, vertex.position);
            bounds_max_ = glm::max(bounds_max_, vertex.position);
        }
    }

    auto StaticColorMesh::index_count() const noexcept -> std::int32_t
    {
        RELAYCORE_PROFILE_FUNCTION();
        return index_count_;
    }

    void StaticColorMesh::reset() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        index_count_ = 0;
        cpu_data_.reset();
    }
} // namespace RelayCore::render
