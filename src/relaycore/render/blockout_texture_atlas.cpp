#include "relaycore/render/blockout_texture_atlas.h"

#include "relaycore/core/log.h"
#include "relaycore/core/paths.h"
#include "relaycore/core/profiling.h"

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glm/common.hpp>

namespace RelayCore::render
{
    namespace
    {
        constexpr int kAtlasColumns = 8;
        constexpr int kAtlasRows = 4;
        constexpr int kTileSize = 64;
        constexpr std::uint8_t kAlpha = 255;
        constexpr int kExpectedWidth = kAtlasColumns * kTileSize;
        constexpr int kExpectedHeight = kAtlasRows * kTileSize;

        template<typename... Args>
        void log_warn_if_available(spdlog::format_string_t<Args...> fmt, Args&&... args)
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (spdlog::default_logger_raw() != nullptr)
            {
                log::warn(fmt, std::forward<Args>(args)...);
            }
        }

        template<typename... Args>
        void log_debug_if_available(spdlog::format_string_t<Args...> fmt, Args&&... args)
        {
            RELAYCORE_PROFILE_FUNCTION();
            if (spdlog::default_logger_raw() != nullptr)
            {
                log::debug(fmt, std::forward<Args>(args)...);
            }
        }

        void write_pixel(
            CpuRgbaTextureData& atlas,
            const int x,
            const int y,
            const std::uint8_t r,
            const std::uint8_t g,
            const std::uint8_t b
        )
        {
            RELAYCORE_PROFILE_FUNCTION();
            const std::size_t index =
                ((static_cast<std::size_t>(y) * static_cast<std::size_t>(atlas.width)) + static_cast<std::size_t>(x))
                * 4;
            atlas.pixels[index + 0] = r;
            atlas.pixels[index + 1] = g;
            atlas.pixels[index + 2] = b;
            atlas.pixels[index + 3] = kAlpha;
        }

        [[nodiscard]] auto vary(const std::uint8_t value, const int delta) -> std::uint8_t
        {
            RELAYCORE_PROFILE_FUNCTION();
            return static_cast<std::uint8_t>(std::clamp(static_cast<int>(value) + delta, 0, 255));
        }

        void fill_tile(CpuRgbaTextureData& atlas, const int tile_index, const std::array<std::uint8_t, 3>& base_color)
        {
            RELAYCORE_PROFILE_FUNCTION();
            const int tile_x = (tile_index % kAtlasColumns) * kTileSize;
            const int tile_y = (tile_index / kAtlasColumns) * kTileSize;

            for (int local_y = 0; local_y < kTileSize; ++local_y)
            {
                for (int local_x = 0; local_x < kTileSize; ++local_x)
                {
                    const bool border =
                        local_x == 0 || local_y == 0 || local_x == (kTileSize - 1) || local_y == (kTileSize - 1);
                    const bool mid_seam = local_x == (kTileSize / 2) || local_y == (kTileSize / 2);
                    const bool micro_grid = (local_x % 4) == 0 || (local_y % 4) == 0;
                    const bool diagonal = ((local_x + local_y) % 7) == 0;
                    const bool slots = (local_y % 3) == 1 && local_x > 1 && local_x < (kTileSize - 2);

                    int shade_delta = ((local_x + local_y) % 5) - 2;
                    if (border)
                    {
                        shade_delta -= 18;
                    }

                    switch (tile_index)
                    {
                        case 3: // glass
                            shade_delta += mid_seam ? 8 : 0;
                            shade_delta += diagonal ? 10 : 0;
                            break;
                        case 4: // hazard
                            shade_delta += ((local_x + local_y) % 6) < 3 ? 14 : -14;
                            break;
                        case 5:  // floor
                        case 10: // platform
                            shade_delta += mid_seam ? -20 : 0;
                            shade_delta += micro_grid ? -8 : 0;
                            break;
                        case 6: // ceiling
                            shade_delta += ((local_x + local_y) % 8 == 0) ? 12 : 0;
                            shade_delta += micro_grid ? -6 : 0;
                            break;
                        case 7: // wall
                        case 8: // wall panel
                        case 9: // corridor
                            shade_delta += mid_seam ? -16 : 0;
                            shade_delta += micro_grid ? -5 : 0;
                            shade_delta += diagonal ? 6 : 0;
                            break;
                        case 11: // platform top
                        case 12: // platform top2
                            shade_delta += mid_seam ? -12 : 0;
                            shade_delta += diagonal ? 7 : 0;
                            break;
                        case 16: // beam
                        case 17: // trim
                            shade_delta += slots ? -22 : 0;
                            shade_delta += micro_grid ? 4 : 0;
                            break;
                        case 18: // divider
                            shade_delta += slots ? 12 : -6;
                            break;
                        case 19: // light strip
                            shade_delta += slots ? 42 : -18;
                            break;
                        case 20: // crate
                        case 15: // block
                            shade_delta += ((local_x / 4) % 2 == 0) ? 10 : -8;
                            shade_delta += (local_y % 5 == 0) ? -10 : 0;
                            break;
                        default:
                            shade_delta += micro_grid ? -4 : 0;
                            break;
                    }

                    write_pixel(
                        atlas,
                        tile_x + local_x,
                        tile_y + local_y,
                        vary(base_color[0], shade_delta),
                        vary(base_color[1], shade_delta),
                        vary(base_color[2], shade_delta)
                    );
                }
            }
        }

        [[nodiscard]] auto to_rgb8(const glm::vec3& color) -> std::array<std::uint8_t, 3>
        {
            const glm::vec3 clamped = glm::clamp(color, glm::vec3{0.0F}, glm::vec3{1.0F});
            return {
                static_cast<std::uint8_t>(clamped.r * 255.0F),
                static_cast<std::uint8_t>(clamped.g * 255.0F),
                static_cast<std::uint8_t>(clamped.b * 255.0F),
            };
        }

        [[nodiscard]] auto generate_blockout_texture_atlas(const RenderAuthoringConfig& authoring_config)
            -> CpuRgbaTextureData
        {
            RELAYCORE_PROFILE_FUNCTION();
            CpuRgbaTextureData atlas{};
            atlas.width = kExpectedWidth;
            atlas.height = kExpectedHeight;
            atlas.pixels.resize(static_cast<std::size_t>(atlas.width) * static_cast<std::size_t>(atlas.height) * 4, 0);

            for (const SurfaceMaterial material : RelayCore::all_surface_materials())
            {
                const std::size_t tile_index = static_cast<std::size_t>(material);
                fill_tile(
                    atlas,
                    static_cast<int>(tile_index),
                    to_rgb8(material_authoring(authoring_config, material).base_color)
                );
            }

            return atlas;
        }
    } // namespace

    auto load_blockout_texture_atlas(const std::filesystem::path& path) -> std::optional<CpuRgbaTextureData>
    {
        RELAYCORE_PROFILE_FUNCTION();
        SDL_Surface* loaded_surface = SDL_LoadBMP(path.string().c_str());
        if (loaded_surface == nullptr)
        {
            log_warn_if_available("[assets] Failed to load blockout atlas '{}': {}", path.string(), SDL_GetError());
            return std::nullopt;
        }

        SDL_Surface* rgba_surface = SDL_ConvertSurface(loaded_surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(loaded_surface);
        if (rgba_surface == nullptr)
        {
            log_warn_if_available(
                "[assets] Failed to convert blockout atlas '{}' to RGBA32: {}",
                path.string(),
                SDL_GetError()
            );
            return std::nullopt;
        }

        CpuRgbaTextureData atlas{};
        atlas.width = rgba_surface->w;
        atlas.height = rgba_surface->h;
        if (atlas.width <= 0 || atlas.height <= 0 || (atlas.width % kAtlasColumns) != 0
            || (atlas.height % kAtlasRows) != 0)
        {
            RELAYCORE_PROFILE_FUNCTION();
            log_warn_if_available(
                "[assets] Blockout atlas '{}' has unsupported dimensions {}x{} (expected dimensions divisible by "
                "{}x{})",
                path.string(),
                atlas.width,
                atlas.height,
                kAtlasColumns,
                kAtlasRows
            );
            SDL_DestroySurface(rgba_surface);
            return std::nullopt;
        }

        atlas.pixels.resize(static_cast<std::size_t>(atlas.width) * static_cast<std::size_t>(atlas.height) * 4);
        const std::size_t row_bytes = static_cast<std::size_t>(atlas.width) * 4;
        for (int y = 0; y < atlas.height; ++y)
        {
            const std::byte* source_row =
                static_cast<const std::byte*>(rgba_surface->pixels)
                + (static_cast<std::size_t>(y) * static_cast<std::size_t>(rgba_surface->pitch));
            std::memcpy(atlas.pixels.data() + (static_cast<std::size_t>(y) * row_bytes), source_row, row_bytes);
        }

        SDL_DestroySurface(rgba_surface);
        log_debug_if_available("[assets] Loaded blockout texture atlas: {}", path.string());
        return atlas;
    }

    auto build_blockout_texture_atlas(const RenderAuthoringConfig& authoring_config) -> CpuRgbaTextureData
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (const auto loaded_atlas = load_blockout_texture_atlas(core::asset_path("textures/blockout_atlas.bmp"));
            loaded_atlas.has_value())
        {
            RELAYCORE_PROFILE_FUNCTION();
            return *loaded_atlas;
        }

        log_warn_if_available("[assets] Using generated fallback blockout atlas");
        return generate_blockout_texture_atlas(authoring_config);
    }

    auto build_blockout_texture_atlas() -> CpuRgbaTextureData
    {
        return build_blockout_texture_atlas(default_render_authoring_config());
    }
} // namespace RelayCore::render
