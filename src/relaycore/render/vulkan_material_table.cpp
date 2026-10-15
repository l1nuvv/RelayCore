#include "relaycore/render/vulkan_material_table.h"

#include "relaycore/render/vulkan_device.h"

#include <algorithm>
#include <stdexcept>

namespace RelayCore::render
{
    namespace
    {
        void require_table_vk(VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
        }
    } // namespace
    VulkanMaterialTable::VulkanMaterialTable(const VulkanDevice& device) : device_(device) {}
    VulkanMaterialTable::~VulkanMaterialTable() noexcept
    {
        if (pool_)
            vkDestroyDescriptorPool(device_.handle(), pool_, nullptr);
        if (layout_)
            vkDestroyDescriptorSetLayout(device_.handle(), layout_, nullptr);
    }
    void VulkanMaterialTable::initialize(uint32_t capacity)
    {
        entries_.resize(capacity);
        const std::array bindings{
            VkDescriptorSetLayoutBinding{
                0,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                1,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                nullptr
            },
            VkDescriptorSetLayoutBinding{
                1,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                capacity * 5,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                nullptr
            }
        };
        const std::array<VkDescriptorBindingFlags, 2> flags{0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
        const VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = uint32_t(flags.size()),
            .pBindingFlags = flags.data()
        };
        const VkDescriptorSetLayoutCreateInfo layout_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &binding_flags,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data()
        };
        require_table_vk(
            vkCreateDescriptorSetLayout(device_.handle(), &layout_info, nullptr, &layout_),
            "Indexed material layout"
        );
        const std::array sizes{
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, capacity * 10}
        };
        const VkDescriptorPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 2,
            .poolSizeCount = uint32_t(sizes.size()),
            .pPoolSizes = sizes.data()
        };
        require_table_vk(
            vkCreateDescriptorPool(device_.handle(), &pool_info, nullptr, &pool_),
            "Indexed material pool"
        );
        const std::array layouts{layout_, layout_};
        const VkDescriptorSetAllocateInfo allocation{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool_,
            .descriptorSetCount = 2,
            .pSetLayouts = layouts.data()
        };
        require_table_vk(
            vkAllocateDescriptorSets(device_.handle(), &allocation, sets_.data()),
            "Indexed material sets"
        );
        for (auto& buffer : surfaces_)
            buffer = VulkanBuffer{
                device_,
                capacity * sizeof(glm::vec4) * 3,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VulkanBufferMemory::upload
            };
    }
    auto VulkanMaterialTable::allocate(
        const std::array<VkDescriptorImageInfo, 5>& images,
        const std::array<glm::vec4, 3>& surface
    ) -> uint32_t
    {
        const auto free = std::find(entries_.begin(), entries_.end(), std::nullopt);
        if (free == entries_.end())
            throw std::runtime_error(
                "Indexed material table exhausted (" + std::to_string(entries_.size())
                + " materials including retained GPU uses); disable render.descriptor_indexing or reduce material count"
            );
        *free = Entry{images, surface};
        ++generation_;
        return uint32_t(free - entries_.begin());
    }
    void VulkanMaterialTable::release(uint32_t index)
    {
        entries_.at(index).reset();
        ++generation_;
    }
    void VulkanMaterialTable::update(size_t slot)
    {
        if (uploaded_[slot] == generation_)
            return;
        if (!entries_[0])
            throw std::logic_error("Indexed material table requires its persistent atlas entry");
        std::vector<VkDescriptorImageInfo> images;
        std::vector<std::array<glm::vec4, 3>> surfaces;
        images.reserve(entries_.size() * 5);
        surfaces.reserve(entries_.size());
        for (const auto& entry : entries_)
        {
            const auto& value = entry ? *entry : *entries_[0];
            images.insert(images.end(), value.images.begin(), value.images.end());
            surfaces.push_back(value.surface);
        }
        surfaces_[slot].write(std::as_bytes(std::span{surfaces}));
        const VkDescriptorBufferInfo buffer{surfaces_[slot].handle(), 0, surfaces_[slot].size()};
        const std::array writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = sets_[slot],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &buffer
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = sets_[slot],
                .dstBinding = 1,
                .descriptorCount = uint32_t(images.size()),
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = images.data()
            }
        };
        vkUpdateDescriptorSets(device_.handle(), uint32_t(writes.size()), writes.data(), 0, nullptr);
        uploaded_[slot] = generation_;
    }
} // namespace RelayCore::render
