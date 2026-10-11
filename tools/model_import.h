#pragma once

#include "relaycore/render/model_asset.h"

namespace RelayCore::tools
{
    [[nodiscard]] auto import_glb(const std::filesystem::path& path, bool metallic_roughness = false)
        -> render::ModelAssetData;
}
