#include "relaycore/render/default_framebuffer_srgb_verifier.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <glad/gl.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RelayCore::render
{
    namespace
    {
        constexpr int kExpectedLinearByte = 128;
        constexpr int kExpectedSrgbByte = 188;
        constexpr int kExpectedOpaqueAlpha = 255;
        constexpr int kByteTolerance = 3;

        void require_no_gl_error(const std::string_view operation)
        {
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR)
            {
                throw std::runtime_error(std::string{operation} + " produced GL error " + std::to_string(error));
            }
        }

        void clear_gl_errors() noexcept
        {
            while (glGetError() != GL_NO_ERROR)
            {}
        }

        void restore_capability(const GLenum capability, const GLboolean enabled) noexcept
        {
            if (enabled == GL_TRUE)
            {
                glEnable(capability);
            } else
            {
                glDisable(capability);
            }
        }

        class DefaultFramebufferStateGuard {
        public:
            DefaultFramebufferStateGuard()
            {
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
                glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer_);
                glGetIntegerv(GL_READ_BUFFER, &read_buffer_);
                glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertex_array_);
                glGetIntegerv(GL_VIEWPORT, viewport_.data());
                glGetIntegerv(GL_SCISSOR_BOX, scissor_box_.data());
                glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color_.data());
                glGetBooleanv(GL_COLOR_WRITEMASK, color_mask_.data());

                blend_enabled_ = glIsEnabled(GL_BLEND);
                cull_enabled_ = glIsEnabled(GL_CULL_FACE);
                depth_enabled_ = glIsEnabled(GL_DEPTH_TEST);
                dither_enabled_ = glIsEnabled(GL_DITHER);
                scissor_enabled_ = glIsEnabled(GL_SCISSOR_TEST);
                framebuffer_srgb_enabled_ = glIsEnabled(GL_FRAMEBUFFER_SRGB);
            }

            ~DefaultFramebufferStateGuard() noexcept
            {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_framebuffer_));
                glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_framebuffer_));
                glDrawBuffer(static_cast<GLenum>(draw_buffer_));
                glReadBuffer(static_cast<GLenum>(read_buffer_));

                glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
                glScissor(scissor_box_[0], scissor_box_[1], scissor_box_[2], scissor_box_[3]);
                glClearColor(clear_color_[0], clear_color_[1], clear_color_[2], clear_color_[3]);
                glColorMask(color_mask_[0], color_mask_[1], color_mask_[2], color_mask_[3]);

                restore_capability(GL_BLEND, blend_enabled_);
                restore_capability(GL_CULL_FACE, cull_enabled_);
                restore_capability(GL_DEPTH_TEST, depth_enabled_);
                restore_capability(GL_DITHER, dither_enabled_);
                restore_capability(GL_SCISSOR_TEST, scissor_enabled_);
                restore_capability(GL_FRAMEBUFFER_SRGB, framebuffer_srgb_enabled_);

                glUseProgram(static_cast<GLuint>(program_));
                glBindVertexArray(static_cast<GLuint>(vertex_array_));
            }

            DefaultFramebufferStateGuard(const DefaultFramebufferStateGuard&) = delete;
            auto operator=(const DefaultFramebufferStateGuard&) -> DefaultFramebufferStateGuard& = delete;

        private:
            GLint draw_framebuffer_ = 0;
            GLint read_framebuffer_ = 0;
            GLint draw_buffer_ = GL_BACK;
            GLint read_buffer_ = GL_BACK;
            GLint program_ = 0;
            GLint vertex_array_ = 0;
            std::array<GLint, 4> viewport_{};
            std::array<GLint, 4> scissor_box_{};
            std::array<GLfloat, 4> clear_color_{};
            std::array<GLboolean, 4> color_mask_{};
            GLboolean blend_enabled_ = GL_FALSE;
            GLboolean cull_enabled_ = GL_FALSE;
            GLboolean depth_enabled_ = GL_FALSE;
            GLboolean dither_enabled_ = GL_FALSE;
            GLboolean scissor_enabled_ = GL_FALSE;
            GLboolean framebuffer_srgb_enabled_ = GL_FALSE;
        };

        [[nodiscard]] auto compile_shader(const GLenum stage, const char* source) -> GLuint
        {
            const GLuint shader = glCreateShader(stage);
            if (shader == 0)
            {
                throw std::runtime_error("default framebuffer sRGB verifier failed to create shader");
            }

            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);

            GLint compiled = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled == GL_TRUE)
            {
                return shader;
            }

            GLint log_length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
            std::string log(static_cast<std::size_t>(std::max(log_length, 1)), '\0');
            glGetShaderInfoLog(shader, log_length, nullptr, log.data());
            glDeleteShader(shader);
            throw std::runtime_error("default framebuffer sRGB verifier shader compilation failed: " + log);
        }

        class VerificationProgram {
        public:
            VerificationProgram()
            {
                constexpr char kVertexShader[] = R"(#version 460 core
void main()
{
    const vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
})";
                constexpr char kFragmentShader[] = R"(#version 460 core
layout (location = 0) out vec4 out_color;
void main()
{
    out_color = vec4(0.5, 0.5, 0.5, 1.0);
})";

                const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, kVertexShader);
                GLuint fragment_shader = 0;
                try
                {
                    fragment_shader = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
                    program_ = glCreateProgram();
                    if (program_ == 0)
                    {
                        throw std::runtime_error("default framebuffer sRGB verifier failed to create program");
                    }

                    glAttachShader(program_, vertex_shader);
                    glAttachShader(program_, fragment_shader);
                    glLinkProgram(program_);
                } catch (...)
                {
                    glDeleteShader(vertex_shader);
                    if (fragment_shader != 0)
                    {
                        glDeleteShader(fragment_shader);
                    }
                    if (program_ != 0)
                    {
                        glDeleteProgram(program_);
                        program_ = 0;
                    }
                    throw;
                }

                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);

                GLint linked = GL_FALSE;
                glGetProgramiv(program_, GL_LINK_STATUS, &linked);
                if (linked != GL_TRUE)
                {
                    GLint log_length = 0;
                    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_length);
                    std::string log(static_cast<std::size_t>(std::max(log_length, 1)), '\0');
                    glGetProgramInfoLog(program_, log_length, nullptr, log.data());
                    glDeleteProgram(program_);
                    program_ = 0;
                    throw std::runtime_error("default framebuffer sRGB verifier program link failed: " + log);
                }

                glGenVertexArrays(1, &vertex_array_);
                if (vertex_array_ == 0)
                {
                    glDeleteProgram(program_);
                    program_ = 0;
                    throw std::runtime_error("default framebuffer sRGB verifier failed to create vertex array");
                }
            }

            ~VerificationProgram() noexcept
            {
                if (vertex_array_ != 0)
                {
                    glDeleteVertexArrays(1, &vertex_array_);
                }
                if (program_ != 0)
                {
                    glDeleteProgram(program_);
                }
            }

            VerificationProgram(const VerificationProgram&) = delete;
            auto operator=(const VerificationProgram&) -> VerificationProgram& = delete;

            void bind() const noexcept
            {
                glUseProgram(program_);
                glBindVertexArray(vertex_array_);
            }

        private:
            GLuint program_ = 0;
            GLuint vertex_array_ = 0;
        };

        [[nodiscard]] auto draw_linear_half(const VerificationProgram& program, const bool enable_srgb)
            -> std::array<std::uint8_t, 4>
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glDrawBuffer(GL_BACK);
            glReadBuffer(GL_BACK);
            glViewport(0, 0, 1, 1);

            glDisable(GL_BLEND);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_DITHER);
            glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            if (enable_srgb)
            {
                glEnable(GL_FRAMEBUFFER_SRGB);
            } else
            {
                glDisable(GL_FRAMEBUFFER_SRGB);
            }

            glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT);
            program.bind();
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glFinish();
            require_no_gl_error(
                enable_srgb ? "enabled default framebuffer sRGB verification draw"
                            : "disabled default framebuffer sRGB verification draw"
            );

            std::array<std::uint8_t, 4> pixel{};
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            require_no_gl_error(
                enable_srgb ? "enabled default framebuffer sRGB verification readback"
                            : "disabled default framebuffer sRGB verification readback"
            );
            return pixel;
        }

        [[nodiscard]] auto near_byte(const std::uint8_t actual, const int expected) noexcept -> bool
        {
            const int difference = static_cast<int>(actual) - expected;
            return (difference < 0 ? -difference : difference) <= kByteTolerance;
        }

        [[nodiscard]] auto rgb_near(const std::array<std::uint8_t, 4>& pixel, const int expected) noexcept -> bool
        {
            return near_byte(pixel[0], expected) && near_byte(pixel[1], expected) && near_byte(pixel[2], expected);
        }
    } // namespace

    auto verify_default_framebuffer_srgb_behavior() -> DefaultFramebufferSrgbVerification
    {
        DefaultFramebufferStateGuard state_guard{};
        VerificationProgram program{};

        clear_gl_errors();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);

        DefaultFramebufferSrgbVerification verification{};
        glGetFramebufferAttachmentParameteriv(
            GL_DRAW_FRAMEBUFFER,
            GL_BACK_LEFT,
            GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
            &verification.reported_encoding
        );
        require_no_gl_error("default framebuffer colour-encoding query");

        verification.disabled_pixel = draw_linear_half(program, false);
        verification.enabled_pixel = draw_linear_half(program, true);
        return verification;
    }

    auto default_framebuffer_srgb_behavior_passes(const DefaultFramebufferSrgbVerification& verification) noexcept
        -> bool
    {
        return rgb_near(verification.disabled_pixel, kExpectedLinearByte)
               && rgb_near(verification.enabled_pixel, kExpectedSrgbByte)
               && near_byte(verification.disabled_pixel[3], kExpectedOpaqueAlpha)
               && near_byte(verification.enabled_pixel[3], kExpectedOpaqueAlpha);
    }

    auto framebuffer_color_encoding_name(const int encoding) noexcept -> const char*
    {
        if (encoding == GL_SRGB)
        {
            return "GL_SRGB";
        }
        if (encoding == GL_LINEAR)
        {
            return "GL_LINEAR";
        }
        return "unknown";
    }

    auto format_rgba8(const std::array<std::uint8_t, 4>& pixel) -> std::string
    {
        return std::to_string(pixel[0]) + "," + std::to_string(pixel[1]) + "," + std::to_string(pixel[2]) + ","
               + std::to_string(pixel[3]);
    }
} // namespace RelayCore::render
