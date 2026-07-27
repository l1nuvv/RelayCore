#include "relaycore/render/texture2d.h"

#include <glad/gl.h>
#include <stdexcept>
#include <utility>

namespace RelayCore::render
{
    Texture2D::~Texture2D() noexcept
    {
        reset();
    }

    Texture2D::Texture2D(Texture2D&& other) noexcept : texture_id_(std::exchange(other.texture_id_, 0)) {}

    auto Texture2D::operator=(Texture2D&& other) noexcept -> Texture2D&
    {
        if (this != &other)
        {
            reset();
            texture_id_ = std::exchange(other.texture_id_, 0);
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
        glGenTextures(1, &texture_id_);
        if (texture_id_ == 0)
        {
            throw std::runtime_error("glGenTextures returned 0");
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        const GLint internal_format = encoding == TextureEncoding::linear ? GL_RGBA8 : GL_SRGB8_ALPHA8;
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Texture2D::bind(const int texture_unit) const
    {
        glActiveTexture(GL_TEXTURE0 + texture_unit);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
    }

    auto Texture2D::valid() const noexcept -> bool
    {
        return texture_id_ != 0;
    }

    void Texture2D::reset() noexcept
    {
        if (texture_id_ != 0)
        {
            glDeleteTextures(1, &texture_id_);
            texture_id_ = 0;
        }
    }
} // namespace RelayCore::render
