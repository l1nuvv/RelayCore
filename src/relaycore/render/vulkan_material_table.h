#pragma once

#include "relaycore/render/vulkan_resources.h"

#include <array>
#include <glm/vec4.hpp>
#include <optional>
#include <vector>

namespace RelayCore::render
{
    class VulkanMaterialTable {
    public:
        explicit VulkanMaterialTable(const VulkanDevice& device);
        ~VulkanMaterialTable() noexcept;
        VulkanMaterialTable(const VulkanMaterialTable&) = delete;
        auto operator=(const VulkanMaterialTable&) -> VulkanMaterialTable& = delete;
        void initialize(uint32_t capacity);
        [[nodiscard]] auto
        allocate(const std::array<VkDescriptorImageInfo, 5>& images, const std::array<glm::vec4, 3>& surface)
            -> uint32_t;
        // Caller has completed the material's final scene and upload use.
        void release(uint32_t index);
        // Caller has waited for this scene slot, including presentation/UI copies.
        void update(size_t slot);
        [[nodiscard]] auto layout() const noexcept -> VkDescriptorSetLayout
        {
            return layout_;
        }
        [[nodiscard]] auto set(size_t slot) const noexcept -> VkDescriptorSet
        {
            return sets_[slot];
        }
        [[nodiscard]] auto capacity() const noexcept -> size_t
        {
            return entries_.size();
        }

    private:
        struct Entry {
            std::array<VkDescriptorImageInfo, 5> images;
            std::array<glm::vec4, 3> surface;
        };
        const VulkanDevice& device_;
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        VkDescriptorPool pool_ = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, 2> sets_{};
        std::array<VulkanBuffer, 2> surfaces_;
        std::vector<std::optional<Entry>> entries_;
        std::array<uint64_t, 2> uploaded_{};
        uint64_t generation_ = 1;
    };
} // namespace RelayCore::render
