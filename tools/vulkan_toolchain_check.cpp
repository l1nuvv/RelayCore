#include <VkBootstrap.h>
#include <cstdio>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

auto main() -> int
{
    const auto system = vkb::SystemInfo::get_system_info(vkGetInstanceProcAddr);
    if (!system)
    {
        std::fprintf(
            stderr,
            "Vulkan loader query failed: %s (VkResult %d)\n",
            system.error().message().c_str(),
            static_cast<int>(system.vk_result())
        );
        return 1;
    }

    const auto version = system->instance_api_version;
    std::printf(
        "Vulkan loader: %u.%u.%u; header revision: %u\n",
        VK_API_VERSION_MAJOR(version),
        VK_API_VERSION_MINOR(version),
        VK_API_VERSION_PATCH(version),
        VK_HEADER_VERSION
    );
    std::printf("VMA header API support: %u.%u\n", VMA_VULKAN_VERSION / 1000000U, VMA_VULKAN_VERSION / 1000U % 1000U);

    if (version < VK_API_VERSION_1_4)
    {
        std::fprintf(stderr, "RelayCore requires Vulkan loader API 1.4 or newer; update the Vulkan driver/loader.\n");
        return 1;
    }

    const bool validation_available = system->is_layer_available("VK_LAYER_KHRONOS_validation");
    const bool debug_utils_available = system->is_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    std::printf(
        "Khronos validation: %s; debug-utils: %s; development checks: %s\n",
        validation_available ? "available" : "unavailable",
        debug_utils_available ? "available" : "unavailable",
        RELAYCORE_VULKAN_VALIDATION ? "enabled" : "disabled"
    );

#if RELAYCORE_VULKAN_VALIDATION
    if (!validation_available || !debug_utils_available)
    {
        std::fprintf(
            stderr,
            "Vulkan development checks require VK_LAYER_KHRONOS_validation and VK_EXT_debug_utils. "
            "Install the Vulkan SDK validation layers or disable RELAYCORE_ENABLE_VULKAN_VALIDATION.\n"
        );
        return 1;
    }
#endif
    return 0;
}
