#pragma once

#include "relaycore/render/mesh.h"

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <span>
#include <vector>

namespace RelayCore::render
{
    enum class RenderPass : std::uint8_t {
        Opaque = 0,
        Overlay,
        Transparent,
    };

    struct LocalLight {
        glm::vec3 position{0.0F};
        glm::vec3 color{0.0F};
        float radius = 1.0F;
    };

    struct RenderItem {
        const StaticColorMesh* mesh = nullptr;
        glm::mat4 model_matrix{1.0F};
        RenderPass pass = RenderPass::Opaque;
    };

    class RenderQueue {
    public:
        void clear() noexcept
        {
            items_.clear();
        }

        void reserve(const std::size_t count)
        {
            items_.reserve(count);
        }

        void
        push(const StaticColorMesh& mesh, const glm::mat4& model_matrix, const RenderPass pass = RenderPass::Opaque)
        {
            items_.push_back(RenderItem{.mesh = &mesh, .model_matrix = model_matrix, .pass = pass});
        }

        [[nodiscard]] auto items() const noexcept -> std::span<const RenderItem>
        {
            return items_;
        }

    private:
        std::vector<RenderItem> items_{};
    };
} // namespace RelayCore::render
