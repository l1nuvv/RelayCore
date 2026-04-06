#pragma once

#include "relaycore/render/texture2d.h"

#include <filesystem>
#include <optional>

namespace RelayCore::render
{
    [[nodiscard]] auto load_blockout_texture_atlas(const std::filesystem::path& path)
        -> std::optional<CpuRgbaTextureData>;

    [[nodiscard]] auto build_blockout_texture_atlas() -> CpuRgbaTextureData;
} // namespace RelayCore::render
