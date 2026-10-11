#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace RelayCore::render
{
    enum class TextureEncoding {
        linear,
        srgb,
    };

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

        void initialize_rgba(int width, int height, std::span<const std::uint8_t> pixels, TextureEncoding encoding);

        [[nodiscard]] auto cpu_data() const noexcept -> const std::shared_ptr<const CpuRgbaTextureData>&
        {
            return cpu_data_;
        }
        [[nodiscard]] auto encoding() const noexcept -> TextureEncoding
        {
            return encoding_;
        }

        [[nodiscard]] auto valid() const noexcept -> bool;

    private:
        void reset() noexcept;

        std::shared_ptr<const CpuRgbaTextureData> cpu_data_;
        TextureEncoding encoding_ = TextureEncoding::linear;
    };
} // namespace RelayCore::render
