#include "relaycore/world/surface_material_utils.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace RelayCore
{
    namespace
    {
        constexpr std::array<SurfaceMaterial, surface_material_count()> kAllSurfaceMaterials{{
            SurfaceMaterial::Concrete,     SurfaceMaterial::Metal,      SurfaceMaterial::Tile,
            SurfaceMaterial::Glass,        SurfaceMaterial::Hazard,     SurfaceMaterial::Floor,
            SurfaceMaterial::Ceiling,      SurfaceMaterial::Wall,       SurfaceMaterial::WallPanel,
            SurfaceMaterial::Corridor,     SurfaceMaterial::Platform,   SurfaceMaterial::PlatformTop,
            SurfaceMaterial::PlatformTop2, SurfaceMaterial::Pillar,     SurfaceMaterial::Cover,
            SurfaceMaterial::Block,        SurfaceMaterial::Beam,       SurfaceMaterial::Trim,
            SurfaceMaterial::Divider,      SurfaceMaterial::LightStrip, SurfaceMaterial::Crate,
            SurfaceMaterial::Post,         SurfaceMaterial::Matte,      SurfaceMaterial::Cotton,
            SurfaceMaterial::Twill,        SurfaceMaterial::Leather,    SurfaceMaterial::Rubber,
        }};

        [[nodiscard]] auto normalize_material_name(std::string_view value) -> std::string
        {
            std::string normalized{};
            normalized.reserve(value.size());
            for (const unsigned char c : value)
            {
                if (c == '-' || c == ' ')
                {
                    normalized.push_back('_');
                    continue;
                }

                normalized.push_back(static_cast<char>(std::tolower(c)));
            }

            return normalized;
        }
    } // namespace

    auto all_surface_materials() noexcept -> const std::array<SurfaceMaterial, surface_material_count()>&
    {
        return kAllSurfaceMaterials;
    }

    auto canonical_surface_material_name(const SurfaceMaterial material) noexcept -> std::string_view
    {
        switch (material)
        {
            case SurfaceMaterial::Cotton:
                return "cotton";
            case SurfaceMaterial::Twill:
                return "twill";
            case SurfaceMaterial::Leather:
                return "leather";
            case SurfaceMaterial::Rubber:
                return "rubber";
            case SurfaceMaterial::Concrete:
                return "concrete";
            case SurfaceMaterial::Metal:
                return "metal";
            case SurfaceMaterial::Tile:
                return "tile";
            case SurfaceMaterial::Glass:
                return "glass";
            case SurfaceMaterial::Hazard:
                return "hazard";
            case SurfaceMaterial::Floor:
                return "floor";
            case SurfaceMaterial::Ceiling:
                return "ceiling";
            case SurfaceMaterial::Wall:
                return "wall";
            case SurfaceMaterial::WallPanel:
                return "wall_panel";
            case SurfaceMaterial::Corridor:
                return "corridor";
            case SurfaceMaterial::Platform:
                return "platform";
            case SurfaceMaterial::PlatformTop:
                return "platform_top";
            case SurfaceMaterial::PlatformTop2:
                return "platform_top_2";
            case SurfaceMaterial::Pillar:
                return "pillar";
            case SurfaceMaterial::Cover:
                return "cover";
            case SurfaceMaterial::Block:
                return "block";
            case SurfaceMaterial::Beam:
                return "beam";
            case SurfaceMaterial::Trim:
                return "trim";
            case SurfaceMaterial::Divider:
                return "divider";
            case SurfaceMaterial::LightStrip:
                return "light_strip";
            case SurfaceMaterial::Crate:
                return "crate";
            case SurfaceMaterial::Matte:
                return "matte";
            case SurfaceMaterial::Post:
                return "post";
        }

        return "unknown";
    }

    auto try_parse_surface_material_name(const std::string_view value) noexcept -> std::optional<SurfaceMaterial>
    {
        const std::string normalized = normalize_material_name(value);
        if (normalized == "cotton")
            return SurfaceMaterial::Cotton;
        if (normalized == "twill")
            return SurfaceMaterial::Twill;
        if (normalized == "leather")
            return SurfaceMaterial::Leather;
        if (normalized == "rubber")
            return SurfaceMaterial::Rubber;

        if (normalized == "concrete")
            return SurfaceMaterial::Concrete;
        if (normalized == "metal")
            return SurfaceMaterial::Metal;
        if (normalized == "tile")
            return SurfaceMaterial::Tile;
        if (normalized == "glass")
            return SurfaceMaterial::Glass;
        if (normalized == "hazard")
            return SurfaceMaterial::Hazard;
        if (normalized == "floor" || normalized == "dev/floor")
            return SurfaceMaterial::Floor;
        if (normalized == "ceiling" || normalized == "dev/ceiling")
            return SurfaceMaterial::Ceiling;
        if (normalized == "wall" || normalized == "dev/wall")
            return SurfaceMaterial::Wall;
        if (normalized == "wall_panel" || normalized == "wallpanel" || normalized == "dev/wall_panel")
            return SurfaceMaterial::WallPanel;
        if (normalized == "corridor" || normalized == "dev/corridor")
            return SurfaceMaterial::Corridor;
        if (normalized == "platform" || normalized == "dev/platform")
            return SurfaceMaterial::Platform;
        if (normalized == "platform_top" || normalized == "platformtop" || normalized == "dev/platform_top")
            return SurfaceMaterial::PlatformTop;
        if (normalized == "platform_top_2" || normalized == "platformtop2" || normalized == "dev/platform_top2")
            return SurfaceMaterial::PlatformTop2;
        if (normalized == "pillar" || normalized == "dev/pillar")
            return SurfaceMaterial::Pillar;
        if (normalized == "cover" || normalized == "dev/cover")
            return SurfaceMaterial::Cover;
        if (normalized == "block" || normalized == "dev/block")
            return SurfaceMaterial::Block;
        if (normalized == "beam" || normalized == "dev/beam")
            return SurfaceMaterial::Beam;
        if (normalized == "trim" || normalized == "dev/trim")
            return SurfaceMaterial::Trim;
        if (normalized == "divider" || normalized == "dev/divider")
            return SurfaceMaterial::Divider;
        if (normalized == "light_strip" || normalized == "lightstrip" || normalized == "dev/light_strip")
            return SurfaceMaterial::LightStrip;
        if (normalized == "crate" || normalized == "dev/crate")
            return SurfaceMaterial::Crate;
        if (normalized == "matte")
            return SurfaceMaterial::Matte;
        if (normalized == "post" || normalized == "dev/post")
            return SurfaceMaterial::Post;

        return std::nullopt;
    }
} // namespace RelayCore
