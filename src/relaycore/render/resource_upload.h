#pragma once

namespace RelayCore::render
{
    // CPU resources are consumed by the Vulkan renderer during the backend overlap.
    enum class ResourceUpload {
        opengl,
        cpu
    };
} // namespace RelayCore::render
