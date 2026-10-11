#include "relaycore/render/vulkan_resources.h"

#include "relaycore/core/profiling.h"
#include "relaycore/render/vulkan_device.h"

#include <algorithm>
#include <bit>
#include <stdexcept>
#include <string>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        void require_resource_result(const VkResult result, const char* operation)
        {
            if (result != VK_SUCCESS)
                throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
        }
    } // namespace

    VulkanBuffer::VulkanBuffer(
        const VulkanDevice& device,
        const VkDeviceSize size,
        const VkBufferUsageFlags usage,
        const VulkanBufferMemory memory
    ) : allocator_(device.allocator()), size_(size), usage_(usage), memory_(memory)
    {
        RELAYCORE_PROFILE_ZONE_N("Vulkan buffer allocation");
        if (size == 0)
            throw std::invalid_argument("Vulkan buffer requires a nonzero size");
        const VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VmaAllocationCreateInfo allocation_info{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        if (memory != VulkanBufferMemory::device)
        {
            allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            allocation_info.flags =
                VMA_ALLOCATION_CREATE_MAPPED_BIT
                | (memory == VulkanBufferMemory::upload ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                        : VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
        }
        require_resource_result(
            vmaCreateBuffer(allocator_, &buffer_info, &allocation_info, &buffer_, &allocation_, nullptr),
            "Failed to allocate Vulkan buffer"
        );
        vmaSetAllocationName(
            allocator_,
            allocation_,
            memory == VulkanBufferMemory::upload     ? "Upload staging buffer"
            : memory == VulkanBufferMemory::readback ? "GPU readback buffer"
                                                     : "Scene/mesh buffer"
        );
    }

    VulkanBuffer::~VulkanBuffer() noexcept
    {
        reset();
    }
    VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    {
        *this = std::move(other);
    }
    auto VulkanBuffer::operator=(VulkanBuffer&& other) noexcept -> VulkanBuffer&
    {
        if (this != &other)
        {
            reset();
            allocator_ = std::exchange(other.allocator_, nullptr);
            buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
            allocation_ = std::exchange(other.allocation_, nullptr);
            size_ = std::exchange(other.size_, 0);
            usage_ = other.usage_;
            upload_timeline_ = std::exchange(other.upload_timeline_, VK_NULL_HANDLE);
            upload_completion_ = std::exchange(other.upload_completion_, 0);
            memory_ = other.memory_;
        }
        return *this;
    }

    void VulkanBuffer::reset() noexcept
    {
        if (buffer_ != VK_NULL_HANDLE)
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
        allocation_ = nullptr;
        size_ = 0;
    }

    void VulkanBuffer::write(const std::span<const std::byte> bytes, const VkDeviceSize offset)
    {
        if (buffer_ == VK_NULL_HANDLE || memory_ != VulkanBufferMemory::upload)
            throw std::logic_error("Vulkan buffer write requires an upload allocation");
        if (offset > size_ || bytes.size_bytes() > size_ - offset)
            throw std::out_of_range("Vulkan buffer write exceeds its allocation");
        if (!bytes.empty())
            require_resource_result(
                vmaCopyMemoryToAllocation(allocator_, bytes.data(), allocation_, offset, bytes.size_bytes()),
                "Failed to write Vulkan upload buffer"
            );
    }

    void VulkanBuffer::read(const std::span<std::byte> bytes, const VkDeviceSize offset) const
    {
        if (buffer_ == VK_NULL_HANDLE || memory_ != VulkanBufferMemory::readback)
            throw std::logic_error("Vulkan buffer read requires a readback allocation");
        if (offset > size_ || bytes.size_bytes() > size_ - offset)
            throw std::out_of_range("Vulkan buffer read exceeds its allocation");
        if (!bytes.empty())
            require_resource_result(
                vmaCopyAllocationToMemory(allocator_, allocation_, offset, bytes.data(), bytes.size_bytes()),
                "Failed to read Vulkan readback buffer"
            );
    }

    VulkanTexture::State::State(const VulkanDevice& owner) : device(owner.handle()), allocator(owner.allocator()) {}
    VulkanTexture::State::~State() noexcept
    {
        if (sampler != VK_NULL_HANDLE)
            vkDestroySampler(device, sampler, nullptr);
        if (view != VK_NULL_HANDLE)
            vkDestroyImageView(device, view, nullptr);
        if (image != VK_NULL_HANDLE)
            vmaDestroyImage(allocator, image, allocation);
    }

    VulkanTexture::VulkanTexture(
        const VulkanDevice& device,
        const int width,
        const int height,
        const TextureEncoding encoding,
        const bool cube
    ) : state_(std::make_unique<State>(device))
    {
        if (width <= 0 || height <= 0)
            throw std::invalid_argument("Vulkan texture requires positive dimensions");
        state_->cube = cube;
        const VkFormat format = cube                                ? VK_FORMAT_R32G32B32A32_SFLOAT
                                : encoding == TextureEncoding::srgb ? VK_FORMAT_R8G8B8A8_SRGB
                                                                    : VK_FORMAT_R8G8B8A8_UNORM;
        constexpr auto usage =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageFormatProperties image_properties{};
        require_resource_result(
            vkGetPhysicalDeviceImageFormatProperties(
                device.physical_device(),
                format,
                VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL,
                usage,
                cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0,
                &image_properties
            ),
            "Vulkan GPU does not support the RGBA texture upload format"
        );
        state_->extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        state_->mip_levels = std::bit_width(std::max(state_->extent.width, state_->extent.height));
        if (state_->extent.width > image_properties.maxExtent.width
            || state_->extent.height > image_properties.maxExtent.height
            || state_->mip_levels > image_properties.maxMipLevels || (cube && image_properties.maxArrayLayers < 6))
            throw std::invalid_argument("Vulkan texture dimensions exceed GPU format limits");
        VkFormatProperties format_properties{};
        vkGetPhysicalDeviceFormatProperties(device.physical_device(), format, &format_properties);
        const VkFormatFeatureFlags required =
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
            | (cube ? 0u : VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
        if ((format_properties.optimalTilingFeatures & required) != required)
            throw std::runtime_error("Vulkan RGBA texture format requires linear filtering and mip blit support");
        const VkImageCreateInfo image_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {state_->extent.width, state_->extent.height, 1},
            .mipLevels = state_->mip_levels,
            .arrayLayers = cube ? 6u : 1u,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        const VmaAllocationCreateInfo allocation_info{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        require_resource_result(
            vmaCreateImage(
                state_->allocator,
                &image_info,
                &allocation_info,
                &state_->image,
                &state_->allocation,
                nullptr
            ),
            "Failed to allocate Vulkan texture"
        );
        vmaSetAllocationName(
            device.allocator(),
            state_->allocation,
            cube ? "Reflection probe cube" : "Material/UI texture"
        );
        const VkImageViewCreateInfo view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = state_->image,
            .viewType = cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, state_->mip_levels, 0, cube ? 6u : 1u},
        };
        require_resource_result(
            vkCreateImageView(state_->device, &view_info, nullptr, &state_->view),
            "Failed to create Vulkan texture view"
        );
        const VkSamplerCreateInfo sampler_info{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = cube ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = cube ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = cube ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = !cube && device.capabilities().max_texture_anisotropy > 1.0F ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = device.capabilities().max_texture_anisotropy,
            .maxLod = static_cast<float>(state_->mip_levels - 1),
        };
        require_resource_result(
            vkCreateSampler(state_->device, &sampler_info, nullptr, &state_->sampler),
            "Failed to create Vulkan texture sampler"
        );
    }

    VulkanTexture::~VulkanTexture() noexcept = default;
    VulkanTexture::VulkanTexture(VulkanTexture&&) noexcept = default;
    auto VulkanTexture::operator=(VulkanTexture&&) noexcept -> VulkanTexture& = default;
    auto VulkanTexture::image() const noexcept -> VkImage
    {
        return state_ ? state_->image : VK_NULL_HANDLE;
    }
    auto VulkanTexture::view() const noexcept -> VkImageView
    {
        return state_ ? state_->view : VK_NULL_HANDLE;
    }
    auto VulkanTexture::sampler() const noexcept -> VkSampler
    {
        return state_ ? state_->sampler : VK_NULL_HANDLE;
    }
    auto VulkanTexture::extent() const noexcept -> VkExtent2D
    {
        return state_ ? state_->extent : VkExtent2D{};
    }
    auto VulkanTexture::mip_levels() const noexcept -> uint32_t
    {
        return state_ ? state_->mip_levels : 0;
    }
} // namespace RelayCore::render
