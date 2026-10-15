#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/reflection_probe.h"
#include "relaycore/render/vulkan_device.h"
#include "relaycore/render/vulkan_scene_submissions.h"
#include "relaycore/render/vulkan_uploads.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "scoped_temp_directory.h"
#include "vulkan_queue_gate.h"

namespace
{
    using namespace RelayCore::render;

    void require_result(const VkResult result, const char* operation)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
    }

    template<typename F>
    void expect_failure(F&& operation, const std::string_view expected)
    {
        try
        {
            operation();
        } catch (const std::exception& error)
        {
            if (std::string_view{error.what()}.find(expected) != std::string_view::npos)
                return;
            throw;
        }
        throw std::runtime_error("Invalid upload data/state was accepted");
    }

    auto allocation_count(const VulkanDevice& device) -> uint32_t
    {
        VmaTotalStatistics stats{};
        vmaCalculateStatistics(device.allocator(), &stats);
        return stats.total.statistics.allocationCount;
    }

    void check_bytes(const std::vector<std::byte>& actual, const std::span<const std::byte> expected)
    {
        if (actual.size() != expected.size() || std::memcmp(actual.data(), expected.data(), expected.size()) != 0)
            throw std::runtime_error("GPU buffer readback differs from uploaded mesh bytes");
    }

    auto checkerboard() -> CpuRgbaTextureData
    {
        CpuRgbaTextureData data{.width = 4, .height = 4};
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
            {
                const auto value = static_cast<uint8_t>((x + y) % 2 == 0 ? 0 : 255);
                data.pixels.insert(data.pixels.end(), {value, value, value, 255});
            }
        return data;
    }

    void check_checker_mips(
        const VulkanTexture& texture,
        const std::vector<std::byte>& bytes,
        const CpuRgbaTextureData& source,
        const TextureEncoding encoding
    )
    {
        if (texture.mip_levels() != 3 || bytes.size() != (16 + 4 + 1) * 4)
            throw std::runtime_error("Checkerboard texture mip chain has incorrect dimensions");
        if (std::memcmp(bytes.data(), source.pixels.data(), source.pixels.size()) != 0)
            throw std::runtime_error("RGBA base mip differs from uploaded bytes");
        const int expected = encoding == TextureEncoding::srgb ? 188 : 128;
        for (size_t i = source.pixels.size(); i < bytes.size(); i += 4)
        {
            for (size_t channel = 0; channel < 3; ++channel)
                if (std::abs(std::to_integer<int>(bytes[i + channel]) - expected) > 1)
                    throw std::runtime_error("Generated mip did not filter in the expected linear/sRGB space");
            if (bytes[i + 3] != std::byte{255})
                throw std::runtime_error("Generated mip lost opaque alpha");
        }
    }

    using RelayCore::test::BlockGraphicsQueue;

    void check_deferred_retirement(const VulkanDevice& device, const CpuColorMeshData& mesh)
    {
        VulkanUploads uploads{device};
        VulkanBufferUpload vertices;
        VulkanBufferUpload indices;
        VulkanTextureUpload texture;
        BlockGraphicsQueue gate{device};
        gate.block();
        vertices =
            uploads.upload_mesh_buffer(std::as_bytes(std::span{mesh.vertices}), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        indices = uploads.upload_mesh_buffer(std::as_bytes(std::span{mesh.indices}), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        texture = uploads.upload_rgba(checkerboard(), TextureEncoding::linear);
        expect_failure(
            [&] {
                uploads.retire(std::move(texture.texture), vertices.completion);
            },
            "at or after"
        );
        {
            VulkanUploads other{device};
            expect_failure(
                [&] {
                    other.retire(std::move(vertices.buffer), vertices.completion);
                },
                "this upload timeline"
            );
        }
        const auto allocated = allocation_count(device);
        uploads.retire(std::move(vertices.buffer), texture.completion);
        uploads.retire(std::move(indices.buffer), texture.completion);
        uploads.retire(std::move(texture.texture), texture.completion);
        uploads.collect();
        if (allocated != 6 || allocation_count(device) != allocated)
            throw std::runtime_error("GPU-pending resources or staging allocations were released before completion");
        const auto pending = uploads.memory_usage();
        const auto expected_staging = mesh.vertices.size() * sizeof(ColorVertex)
                                      + mesh.indices.size() * sizeof(uint32_t) + checkerboard().pixels.size();
        if (pending.staging_bytes != expected_staging || pending.batches != 3 || pending.retirements != 3)
            throw std::runtime_error("Upload telemetry lost blocked staging bytes/batches/retirements");
        gate.release();
        uploads.wait(texture.completion);
        uploads.collect();
        if (allocation_count(device) != 0)
            throw std::runtime_error("Completed upload retirement did not release all VMA allocations");
        const auto settled = uploads.memory_usage();
        if (settled.staging_bytes || settled.batches || settled.retirements)
            throw std::runtime_error("Upload telemetry retained collected allocations");
        std::cout << "upload_memory: blocked_bytes=" << pending.staging_bytes << "; batches=" << pending.batches
                  << "; retirements=" << pending.retirements << "; settled=0\n";
    }

    void check_scene_retirement(const VulkanDevice& device, const CpuColorMeshData& mesh)
    {
        struct Commands {
            const VulkanDevice& device;
            VkCommandPool pool = VK_NULL_HANDLE;
            ~Commands()
            {
                if (pool)
                    vkDestroyCommandPool(device.handle(), pool, nullptr);
            }
        } commands{device};
        const VkCommandPoolCreateInfo pool{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = device.capabilities().graphics_queue_family
        };
        require_result(vkCreateCommandPool(device.handle(), &pool, nullptr, &commands.pool), "Scene test pool");
        std::array<VkCommandBuffer, 2> buffers{};
        const VkCommandBufferAllocateInfo allocate{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commands.pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 2
        };
        require_result(vkAllocateCommandBuffers(device.handle(), &allocate, buffers.data()), "Scene test commands");
        VulkanUploads uploads{device};
        auto vertices =
            uploads.upload_mesh_buffer(std::as_bytes(std::span{mesh.vertices}), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        auto texture = uploads.upload_rgba(checkerboard(), TextureEncoding::linear);
        uploads.wait(texture.completion);
        uploads.collect();
        VulkanBuffer
            readback{device, vertices.buffer.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VulkanBufferMemory::readback};
        VulkanBuffer pixels{device, 4 * 4 * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VulkanBufferMemory::readback};
        const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        require_result(vkBeginCommandBuffer(buffers[0], &begin), "Scene buffer begin");
        const VkBufferCopy copy{0, 0, vertices.buffer.size()};
        vkCmdCopyBuffer(buffers[0], vertices.buffer.handle(), readback.handle(), 1, &copy);
        require_result(vkEndCommandBuffer(buffers[0]), "Scene buffer end");
        require_result(vkBeginCommandBuffer(buffers[1], &begin), "Scene texture begin");
        const VkImageMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = texture.texture.image(),
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        const VkDependencyInfo dependency{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        };
        vkCmdPipelineBarrier2(buffers[1], &dependency);
        const VkBufferImageCopy image_copy{
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageExtent = {4, 4, 1}
        };
        vkCmdCopyImageToBuffer(
            buffers[1],
            texture.texture.image(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            pixels.handle(),
            1,
            &image_copy
        );
        require_result(vkEndCommandBuffer(buffers[1]), "Scene texture end");
        {
            VulkanSceneSubmissions scene{device};
            BlockGraphicsQueue gate{device};
            gate.block();
            expect_failure(
                [&] {
                    static_cast<void>(scene.submit(VK_NULL_HANDLE));
                },
                "command buffer"
            );
            if (scene.last_submission().value != 0)
                throw std::runtime_error("Rejected scene submission published a completion");
            const auto first = scene.submit(buffers[0]);
            const auto last = scene.submit(buffers[1]);
            if (first.value != 1 || last.value != 2 || scene.completed() != 0)
                throw std::runtime_error("Blocked scene timeline advanced or lost submission order");
            expect_failure(
                [&] {
                    scene.wait({last.timeline, last.value + 1});
                },
                "submitted value"
            );
            expect_failure(
                [&] {
                    scene.retire(std::move(vertices.buffer), {last.timeline, last.value + 1});
                },
                "submitted value"
            );
            {
                VulkanSceneSubmissions other{device};
                expect_failure(
                    [&] {
                        other.retire(std::move(vertices.buffer), first);
                    },
                    "this timeline"
                );
            }
            if (!vertices.buffer.handle())
                throw std::runtime_error("Rejected scene retirement took resource ownership");
            scene.retire(std::move(vertices.buffer), first);
            scene.retire(std::move(texture.texture), last);
            scene.retire(std::move(readback), first);
            scene.retire(std::move(pixels), last);
            scene.collect();
            if (allocation_count(device) != 4)
                throw std::runtime_error("Scene retirement freed GPU-pending resources");
            if (scene.pending_retirements() != 4)
                throw std::runtime_error("Scene telemetry lost GPU-pending retirements");
            gate.release();
            scene.wait(last);
            scene.collect();
            if (allocation_count(device) != 0)
                throw std::runtime_error("Scene retirement retained completed resources");
            if (scene.pending_retirements() != 0)
                throw std::runtime_error("Scene telemetry retained collected resources");
        }
        {
            auto uploaded =
                uploads.upload_mesh_buffer(std::as_bytes(std::span{mesh.vertices}), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            uploads.wait(uploaded.completion);
            uploads.collect();
            // Re-record the completed command to retain a real pending buffer reference at owner shutdown.
            require_result(vkResetCommandPool(device.handle(), commands.pool, 0), "Scene shutdown reset");
            VulkanBuffer destination{
                device,
                uploaded.buffer.size(),
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VulkanBufferMemory::readback
            };
            require_result(vkBeginCommandBuffer(buffers[0], &begin), "Scene shutdown begin");
            vkCmdCopyBuffer(buffers[0], uploaded.buffer.handle(), destination.handle(), 1, &copy);
            require_result(vkEndCommandBuffer(buffers[0]), "Scene shutdown end");
            BlockGraphicsQueue gate{device};
            gate.block();
            VulkanSceneSubmissions scene{device};
            try
            {
                const auto last = scene.submit(buffers[0]);
                scene.retire(std::move(uploaded.buffer), last);
                scene.retire(std::move(destination), last);
                gate.release();
            } catch (...)
            {
                gate.release();
                throw;
            }
            // No explicit scene wait/collect; its destructor must drain before freeing resources.
        }
        {
            require_result(vkResetCommandPool(device.handle(), commands.pool, 0), "GPU upload consume reset");
            VulkanSceneSubmissions scene{device};
            BlockGraphicsQueue gate{device};
            auto uploaded = VulkanBufferUpload{};
            VulkanBuffer destination{device, copy.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VulkanBufferMemory::readback};
            gate.block();
            try
            {
                uploaded = uploads.upload_mesh_buffer(
                    std::as_bytes(std::span{mesh.vertices}),
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                );
                require_result(vkBeginCommandBuffer(buffers[0], &begin), "GPU upload consume begin");
                vkCmdCopyBuffer(buffers[0], uploaded.buffer.handle(), destination.handle(), 1, &copy);
                const VkMemoryBarrier2 host{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                    .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT
                };
                const VkDependencyInfo host_dependency{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .memoryBarrierCount = 1,
                    .pMemoryBarriers = &host
                };
                vkCmdPipelineBarrier2(buffers[0], &host_dependency);
                require_result(vkEndCommandBuffer(buffers[0]), "GPU upload consume end");
                const auto completion = scene.submit(buffers[0], uploads.dependency());
                if (scene.completed() >= completion.value || uploads.completed() >= uploaded.completion)
                    throw std::runtime_error("GPU upload dependency required a host wait");
                gate.release();
                scene.wait(completion);
                std::vector<std::byte> bytes(copy.size);
                destination.read(bytes);
                check_bytes(bytes, std::as_bytes(std::span{mesh.vertices}));
            } catch (...)
            {
                gate.release();
                scene.wait(scene.last_submission());
                uploads.wait(uploads.dependency().value);
                throw;
            }
        }
        uploads.collect();
        if (allocation_count(device) != 0)
            throw std::runtime_error("Scene shutdown leaked VMA resources");
    }

    void check_uploads(const VulkanDevice& device)
    {
        const auto mesh = make_color_box_mesh_data();
        {
            VulkanUploads uploads{device};
            expect_failure(
                [&] {
                    static_cast<void>(uploads.upload_mesh_buffer({}, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
                },
                "nonempty"
            );
            expect_failure(
                [&] {
                    static_cast<void>(uploads.upload_rgba({.width = -1, .height = 4}, TextureEncoding::linear));
                },
                "positive"
            );
            expect_failure(
                [&] {
                    static_cast<void>(uploads.upload_rgba({.width = 4, .height = 4}, TextureEncoding::linear));
                },
                "size mismatch"
            );
            expect_failure(
                [&] {
                    uploads.wait(1);
                },
                "not been submitted"
            );
            VulkanBuffer mapped{device, 16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VulkanBufferMemory::upload};
            const std::array<std::byte, 4> value{std::byte{3}, std::byte{7}, std::byte{11}, std::byte{19}};
            expect_failure(
                [&] {
                    mapped.write(value, 14);
                },
                "exceeds"
            );
            mapped.write(value, 12);
            const auto bytes = uploads.readback(mapped);
            if (!std::equal(value.begin(), value.end(), bytes.begin() + 12))
                throw std::runtime_error("Mapped upload offset/flush path did not preserve written bytes");
        }
        for (const auto encoding : {TextureEncoding::linear, TextureEncoding::srgb})
        {
            VulkanUploads uploads{device};
            VulkanTextureUpload previous;
            for (int reload = 0; reload < 3; ++reload)
            {
                auto vertices = uploads.upload_mesh_buffer(
                    std::as_bytes(std::span{mesh.vertices}),
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                );
                auto indices = uploads.upload_mesh_buffer(
                    std::as_bytes(std::span{mesh.indices}),
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                );
                const auto source = checkerboard();
                auto texture = uploads.upload_rgba(source, encoding);
                if (previous.texture.image() != VK_NULL_HANDLE)
                    uploads.retire(std::move(previous.texture), previous.completion);
                check_bytes(uploads.readback(vertices.buffer), std::as_bytes(std::span{mesh.vertices}));
                check_bytes(uploads.readback(indices.buffer), std::as_bytes(std::span{mesh.indices}));
                check_checker_mips(texture.texture, uploads.readback(texture.texture), source, encoding);
                check_checker_mips(texture.texture, uploads.readback(texture.texture), source, encoding);
                uploads.retire(std::move(vertices.buffer), texture.completion);
                uploads.retire(std::move(indices.buffer), texture.completion);
                previous = std::move(texture);
                uploads.collect();
            }
            uploads.retire(std::move(previous.texture), previous.completion);
            uploads.collect();
            CpuRgbaTextureData narrow{.width = 7, .height = 3};
            for (int pixel = 0; pixel < 21; ++pixel)
                narrow.pixels.insert(narrow.pixels.end(), {23, 63, 111, 255});
            auto texture = uploads.upload_rgba(narrow, encoding);
            const auto bytes = uploads.readback(texture.texture);
            if (bytes.size() != (21 + 3 + 1) * 4)
                throw std::runtime_error("Non-power-of-two mip extents were truncated incorrectly");
            for (size_t i = 0; i < bytes.size(); ++i)
                if (std::abs(std::to_integer<int>(bytes[i]) - narrow.pixels[i % 4]) > 1)
                    throw std::runtime_error("Non-power-of-two constant texture changed during mip generation");
            uploads.retire(std::move(texture.texture), texture.completion);
            uploads.collect();
        }
        {
            VulkanUploads uploads{device};
            CpuRgbaTextureData detailed{.width = 2048, .height = 1024};
            detailed.pixels.resize(static_cast<size_t>(detailed.width) * detailed.height * 4);
            for (int y = 0; y < detailed.height; ++y)
                for (int x = 0; x < detailed.width; ++x)
                {
                    const auto offset = (static_cast<size_t>(y) * detailed.width + x) * 4;
                    detailed.pixels[offset] = static_cast<uint8_t>(x);
                    detailed.pixels[offset + 1] = static_cast<uint8_t>(y);
                    detailed.pixels[offset + 2] = static_cast<uint8_t>((x + y) % 2 * 255);
                    detailed.pixels[offset + 3] = 255;
                }
            auto texture = uploads.upload_rgba(detailed, TextureEncoding::srgb);
            const auto bytes = uploads.readback(texture.texture);
            if (texture.texture.extent().width != 2048 || texture.texture.extent().height != 1024
                || texture.texture.mip_levels() != 12 || bytes.size() < detailed.pixels.size()
                || std::memcmp(bytes.data(), detailed.pixels.data(), detailed.pixels.size()) != 0)
                throw std::runtime_error("Detailed model-size texture lost resolution or texels during GPU upload");
            uploads.retire(std::move(texture.texture), texture.completion);
            uploads.collect();
        }
        if (allocation_count(device) != 0)
            throw std::runtime_error("Upload/readback/reload leaked VMA allocations");
        {
            VulkanUploads uploads{device};
            ReflectionProbe cube{{{}, {-2, -2, -2}, {2, 2, 2}, 8, 32}, 0, {}};
            std::vector<std::byte> expected;
            for (uint32_t size = 8; size; size >>= 1)
            {
                auto& mip = cube.mips.emplace_back(6 * size * size);
                for (std::size_t pixel = 0; pixel < mip.size(); ++pixel)
                    mip[pixel] = {static_cast<float>(pixel) / (size * size) + .1F, static_cast<float>(size), .5F, 1};
                const auto bytes = std::as_bytes(std::span{mip});
                expected.insert(expected.end(), bytes.begin(), bytes.end());
            }
            auto uploaded = uploads.upload_reflection(cube);
            check_bytes(uploads.readback(uploaded.texture), expected);
            uploads.retire(std::move(uploaded.texture), uploaded.completion);
            uploads.collect();
        }
        if (allocation_count(device) != 0)
            throw std::runtime_error("Reflection cube upload/readback leaked VMA allocations");
        check_deferred_retirement(device, mesh);
        check_scene_retirement(device, mesh);
        {
            VulkanUploads uploads{device};
            auto texture = uploads.upload_rgba(checkerboard(), TextureEncoding::srgb);
            uploads.retire(std::move(texture.texture), texture.completion);
            // Shutdown must drain its own timeline even without an explicit wait/collect.
        }
        if (allocation_count(device) != 0)
            throw std::runtime_error("Upload owner shutdown leaked pending VMA allocations");
    }
} // namespace

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "vulkan_upload=skipped: " << SDL_GetError() << '\n';
        return 77;
    }
    int result = 0;
    try
    {
        const RelayCore::test::ScopedTempDirectory temp;
        RelayCore::log::init({.logs_directory = temp.path().string(), .enable_timestamped_log = false});
        RelayCore::Window window{{.width = 64, .height = 64}};
        VulkanDevice device{window};
        check_uploads(device);
        std::cout << "vulkan_upload=passed: mesh bytes, linear/sRGB mips, reload, mapped offsets, deferred retirement, "
                     "shutdown\n";
    } catch (const std::exception& error)
    {
        std::cerr << "Vulkan upload failed: " << error.what() << '\n';
        result = 1;
    }
    RelayCore::log::shutdown();
    SDL_Quit();
    return result;
}
