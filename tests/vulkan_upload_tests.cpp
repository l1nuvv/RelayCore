#include "relaycore/core/log.h"
#include "relaycore/platform/window.h"
#include "relaycore/render/mesh.h"
#include "relaycore/render/reflection_probe.h"
#include "relaycore/render/vulkan_device.h"
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

    // A host-signaled timeline blocks actual GPU execution, making early retirement checks deterministic.
    class BlockGraphicsQueue {
    public:
        explicit BlockGraphicsQueue(const VulkanDevice& device) : device_(device)
        {
            const VkSemaphoreTypeCreateInfo type{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE
            };
            const VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type};
            require_result(
                vkCreateSemaphore(device_.handle(), &info, nullptr, &gate_),
                "Failed to create GPU test gate"
            );
        }
        void block()
        {
            const VkSemaphoreSubmitInfo wait{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = gate_,
                .value = 1,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
            };
            const VkSubmitInfo2 submit{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = 1,
                .pWaitSemaphoreInfos = &wait
            };
            require_result(
                vkQueueSubmit2(device_.graphics_queue(), 1, &submit, VK_NULL_HANDLE),
                "Failed to block GPU test queue"
            );
        }
        void release()
        {
            const VkSemaphoreSignalInfo signal{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
                .semaphore = gate_,
                .value = 1
            };
            require_result(vkSignalSemaphore(device_.handle(), &signal), "Failed to release GPU test gate");
            released_ = true;
        }
        ~BlockGraphicsQueue() noexcept
        {
            if (!released_)
            {
                const VkSemaphoreSignalInfo signal{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
                    .semaphore = gate_,
                    .value = 1
                };
                static_cast<void>(vkSignalSemaphore(device_.handle(), &signal));
            }
            // Test-only cleanup releases queued waits even when an assertion throws.
            static_cast<void>(vkQueueWaitIdle(device_.graphics_queue()));
            vkDestroySemaphore(device_.handle(), gate_, nullptr);
        }

    private:
        const VulkanDevice& device_;
        VkSemaphore gate_ = VK_NULL_HANDLE;
        bool released_ = false;
    };

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
        gate.release();
        uploads.wait(texture.completion);
        uploads.collect();
        if (allocation_count(device) != 0)
            throw std::runtime_error("Completed upload retirement did not release all VMA allocations");
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
