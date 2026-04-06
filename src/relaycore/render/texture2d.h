#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace RelayCore::render
{
    struct CpuRgbaTextureData {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> pixels{};
    };

    class Texture2D {
    public:
        Texture2D() = default;

        ~Texture2D() noexcept;

        Texture2D(const Texture2D&) = delete;

        auto operator=(const Texture2D&) -> Texture2D& = delete;

        Texture2D(Texture2D&& other) noexcept;

        auto operator=(Texture2D&& other) noexcept -> Texture2D&;

        void initialize_rgba(int width, int height, std::span<const std::uint8_t> pixels);

        void bind(int texture_unit) const;

        [[nodiscard]] auto valid() const noexcept -> bool;

    private:
        void reset() noexcept;

        unsigned int texture_id_ = 0;
    };
} // namespace RelayCore::render
