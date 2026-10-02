#include "relaycore/render/texture2d.h"

#include <stdexcept>
#include <utility>

namespace RelayCore::render
{
    Texture2D::~Texture2D() noexcept
    {
        reset();
    }

    Texture2D::Texture2D(Texture2D&& other) noexcept : cpu_data_(std::move(other.cpu_data_)), encoding_(other.encoding_)
    {}

    auto Texture2D::operator=(Texture2D&& other) noexcept -> Texture2D&
    {
        if (this != &other)
        {
            reset();
            cpu_data_ = std::move(other.cpu_data_);
            encoding_ = other.encoding_;
        }

        return *this;
    }

    void Texture2D::initialize_rgba(
        const int width,
        const int height,
        const std::span<const std::uint8_t> pixels,
        const TextureEncoding encoding
    )
    {
        if (width <= 0 || height <= 0)
        {
            throw std::runtime_error("Texture2D requires positive dimensions");
        }
        if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4)
        {
            throw std::runtime_error("Texture2D RGBA upload size mismatch");
        }

        reset();
        encoding_ = encoding;
        cpu_data_ =
            std::make_shared<CpuRgbaTextureData>(CpuRgbaTextureData{width, height, {pixels.begin(), pixels.end()}});
    }

    auto Texture2D::valid() const noexcept -> bool
    {
        return cpu_data_ != nullptr;
    }

    void Texture2D::reset() noexcept
    {
        cpu_data_.reset();
    }
} // namespace RelayCore::render
