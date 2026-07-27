#include "relaycore/render/texture2d.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <glad/gl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr int kRampWidth = 5;
    constexpr int kReferenceHeight = 1;
    constexpr std::array<std::uint8_t, kRampWidth> kExpectedFullLight{0, 63, 127, 191, 255};
    constexpr std::array<std::uint8_t, kRampWidth> kExpectedHalfLight{0, 31, 63, 95, 127};
    constexpr std::array<std::uint8_t, 4> kBlendSource{204, 51, 102, 128};
    constexpr std::array<std::uint8_t, 3> kBlendDestination{64, 128, 192};
    constexpr std::array<std::uint8_t, 3> kExpectedBlend{134, 90, 147};
    constexpr int kByteTolerance = 1;
    constexpr int kSrgbByteTolerance = 2;
    constexpr float kFloatTolerance = 0.002F;
    constexpr int kSkippedExitCode = 77;

    class BaselineUnavailable final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class SdlVideoLifetime {
    public:
        SdlVideoLifetime()
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
            {
                throw BaselineUnavailable("SDL video initialization failed: " + std::string{SDL_GetError()});
            }
        }

        ~SdlVideoLifetime() noexcept
        {
            SDL_Quit();
        }

        SdlVideoLifetime(const SdlVideoLifetime&) = delete;
        auto operator=(const SdlVideoLifetime&) -> SdlVideoLifetime& = delete;
    };

    class HiddenGlContext {
    public:
        HiddenGlContext()
        {
            require_sdl(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4), "set GL major version");
            require_sdl(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6), "set GL minor version");
            require_sdl(
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE),
                "set GL core profile"
            );
            require_sdl(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1), "set GL double buffering");

            window_ =
                SDL_CreateWindow("RelayCore graphics baseline", kRampWidth, 1, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
            if (window_ == nullptr)
            {
                throw BaselineUnavailable("hidden SDL OpenGL window creation failed: " + std::string{SDL_GetError()});
            }

            context_ = SDL_GL_CreateContext(window_);
            if (context_ == nullptr)
            {
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                throw BaselineUnavailable("OpenGL context creation failed: " + std::string{SDL_GetError()});
            }

            if (gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) == 0)
            {
                SDL_GL_DestroyContext(context_);
                context_ = nullptr;
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                throw BaselineUnavailable("gladLoadGL failed");
            }
        }

        ~HiddenGlContext() noexcept
        {
            if (context_ != nullptr)
            {
                SDL_GL_DestroyContext(context_);
            }
            if (window_ != nullptr)
            {
                SDL_DestroyWindow(window_);
            }
        }

        HiddenGlContext(const HiddenGlContext&) = delete;
        auto operator=(const HiddenGlContext&) -> HiddenGlContext& = delete;

    private:
        static void require_sdl(const bool success, const std::string_view operation)
        {
            if (!success)
            {
                throw BaselineUnavailable(std::string{operation} + " failed: " + SDL_GetError());
            }
        }

        SDL_Window* window_ = nullptr;
        SDL_GLContext context_ = nullptr;
    };

    class GlProgram {
    public:
        GlProgram(const char* vertex_source, const char* fragment_source)
        {
            const GLuint vertex_shader = compile(GL_VERTEX_SHADER, vertex_source);
            GLuint fragment_shader = 0;
            try
            {
                fragment_shader = compile(GL_FRAGMENT_SHADER, fragment_source);
            } catch (...)
            {
                glDeleteShader(vertex_shader);
                throw;
            }
            program_ = glCreateProgram();
            if (program_ == 0)
            {
                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);
                throw std::runtime_error("controlled baseline program creation failed");
            }
            glAttachShader(program_, vertex_shader);
            glAttachShader(program_, fragment_shader);
            glLinkProgram(program_);
            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);

            GLint linked = GL_FALSE;
            glGetProgramiv(program_, GL_LINK_STATUS, &linked);
            if (linked != GL_TRUE)
            {
                glDeleteProgram(program_);
                program_ = 0;
                throw std::runtime_error("controlled baseline shader link failed");
            }
        }

        ~GlProgram() noexcept
        {
            if (program_ != 0)
            {
                glDeleteProgram(program_);
            }
        }

        void use() const
        {
            glUseProgram(program_);
        }

        [[nodiscard]] auto uniform(const char* name) const -> GLint
        {
            return glGetUniformLocation(program_, name);
        }

    private:
        [[nodiscard]] static auto compile(const GLenum stage, const char* source) -> GLuint
        {
            const GLuint shader = glCreateShader(stage);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);
            GLint compiled = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled != GL_TRUE)
            {
                glDeleteShader(shader);
                throw std::runtime_error("controlled baseline shader compilation failed");
            }
            return shader;
        }

        GLuint program_ = 0;
    };

    class GlVertexArray {
    public:
        GlVertexArray()
        {
            glGenVertexArrays(1, &id_);
            glBindVertexArray(id_);
        }

        ~GlVertexArray() noexcept
        {
            if (id_ != 0)
            {
                glDeleteVertexArrays(1, &id_);
            }
        }

        GlVertexArray(const GlVertexArray&) = delete;
        auto operator=(const GlVertexArray&) -> GlVertexArray& = delete;

    private:
        GLuint id_ = 0;
    };

    void require_no_gl_error(std::string_view operation);

    class GlTextureAttachment {
    public:
        GlTextureAttachment(const int width, const int height, const GLenum internal_format, const GLenum type)
        {
            glGenTextures(1, &id_);
            glBindTexture(GL_TEXTURE_2D, id_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, type, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);
            require_no_gl_error("controlled FBO texture attachment creation");
        }

        ~GlTextureAttachment() noexcept
        {
            if (id_ != 0)
            {
                glDeleteTextures(1, &id_);
            }
        }

        [[nodiscard]] auto id() const noexcept -> GLuint
        {
            return id_;
        }

    private:
        GLuint id_ = 0;
    };

    class GlFramebuffer {
    public:
        explicit GlFramebuffer(const GlTextureAttachment& attachment)
        {
            glGenFramebuffers(1, &id_);
            glBindFramebuffer(GL_FRAMEBUFFER, id_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, attachment.id(), 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw std::runtime_error("controlled framebuffer is incomplete");
            }
            require_no_gl_error("controlled framebuffer setup");
        }

        ~GlFramebuffer() noexcept
        {
            if (id_ != 0)
            {
                glDeleteFramebuffers(1, &id_);
            }
        }

        void bind() const
        {
            glBindFramebuffer(GL_FRAMEBUFFER, id_);
        }

    private:
        GLuint id_ = 0;
    };

    void require_no_gl_error(const std::string_view operation)
    {
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            throw std::runtime_error(std::string{operation} + " produced GL error " + std::to_string(error));
        }
    }

    void require_near(const int actual, const int expected, const std::string_view label)
    {
        if (std::abs(actual - expected) > kByteTolerance)
        {
            throw std::runtime_error(
                std::string{label} + " expected " + std::to_string(expected) + ", got " + std::to_string(actual)
            );
        }
    }

    void require_near(const float actual, const float expected, const std::string_view label)
    {
        if (std::abs(actual - expected) > kFloatTolerance)
        {
            throw std::runtime_error(
                std::string{label} + " expected " + std::to_string(expected) + ", got " + std::to_string(actual)
            );
        }
    }

    void require_srgb_near(const int actual, const int expected, const std::string_view label)
    {
        if (std::abs(actual - expected) > kSrgbByteTolerance)
        {
            throw std::runtime_error(
                std::string{label} + " expected " + std::to_string(expected) + ", got " + std::to_string(actual)
            );
        }
    }

    [[nodiscard]] auto srgb_to_linear(const float encoded) -> float
    {
        return encoded <= 0.04045F ? encoded / 12.92F : std::pow((encoded + 0.055F) / 1.055F, 2.4F);
    }

    [[nodiscard]] auto linear_to_srgb_byte(const float linear) -> int
    {
        const float encoded = linear <= 0.0031308F ? linear * 12.92F : 1.055F * std::pow(linear, 1.0F / 2.4F) - 0.055F;
        return static_cast<int>(std::lround(encoded * 255.0F));
    }

    void print_gl_string(const char* label, const GLenum name)
    {
        const auto* value = reinterpret_cast<const char*>(glGetString(name));
        std::cout << label << '=' << (value != nullptr ? value : "unavailable") << '\n';
    }

    struct FramebufferBits {
        int red = 0;
        int green = 0;
        int blue = 0;
        int alpha = 0;
    };

    void configure_default_framebuffer_path()
    {
        glDrawBuffer(GL_BACK);
        if (const GLenum error = glGetError(); error != GL_NO_ERROR)
        {
            throw BaselineUnavailable(
                "default framebuffer does not support GL_BACK draw buffer (GL error " + std::to_string(error) + ')'
            );
        }
        glReadBuffer(GL_BACK);
        if (const GLenum error = glGetError(); error != GL_NO_ERROR)
        {
            throw BaselineUnavailable(
                "default framebuffer does not support GL_BACK read buffer (GL error " + std::to_string(error) + ')'
            );
        }
        glDisable(GL_DITHER);
        glDisable(GL_FRAMEBUFFER_SRGB);
        require_no_gl_error("default framebuffer baseline state setup");
        if (glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE)
        {
            throw std::runtime_error("GL_FRAMEBUFFER_SRGB must remain disabled in the RC-022a baseline");
        }
    }

    [[nodiscard]] auto print_default_framebuffer_inventory() -> FramebufferBits
    {
        print_gl_string("GL_VERSION", GL_VERSION);
        print_gl_string("GL_VENDOR", GL_VENDOR);
        print_gl_string("GL_RENDERER", GL_RENDERER);

        FramebufferBits bits{};
        if (!SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &bits.red) || !SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &bits.green)
            || !SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &bits.blue)
            || !SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &bits.alpha))
        {
            throw BaselineUnavailable("default framebuffer bit-depth query failed: " + std::string{SDL_GetError()});
        }
        std::cout << "default_framebuffer_bits=" << bits.red << ',' << bits.green << ',' << bits.blue << ','
                  << bits.alpha << '\n';
        std::cout << "GL_FRAMEBUFFER_SRGB=" << (glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE ? "enabled" : "disabled")
                  << '\n';

        GLint color_encoding = 0;
        glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER,
            GL_BACK_LEFT,
            GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
            &color_encoding
        );
        const GLenum encoding_error = glGetError();
        if (encoding_error == GL_NO_ERROR)
        {
            std::cout << "default_framebuffer_color_encoding=" << (color_encoding == GL_SRGB ? "GL_SRGB" : "GL_LINEAR")
                      << '\n';
        } else
        {
            std::cout << "default_framebuffer_color_encoding=unavailable (GL error " << encoding_error << ")\n";
        }
        std::cout << "sRGB_capable_default_framebuffer_contract=not_requested_by_RelayCore\n";
        return bits;
    }

    void draw_fullscreen_triangle()
    {
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    [[nodiscard]] auto read_rgb_row() -> std::array<std::array<std::uint8_t, 3>, kRampWidth>
    {
        std::array<std::array<std::uint8_t, 3>, kRampWidth> pixels{};
        glReadPixels(0, 0, kRampWidth, kReferenceHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        require_no_gl_error("gray ramp glReadPixels");
        return pixels;
    }

    void verify_gray_ramp(
        const RelayCore::render::Texture2D& texture,
        const GlProgram& program,
        const float multiplier,
        const std::array<std::uint8_t, kRampWidth>& expected,
        const std::string_view label
    )
    {
        glDisable(GL_BLEND);
        glViewport(0, 0, kRampWidth, kReferenceHeight);
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);

        program.use();
        texture.bind(0);
        glUniform1i(program.uniform("u_reference_texture"), 0);
        glUniform1f(program.uniform("u_multiplier"), multiplier);
        draw_fullscreen_triangle();
        require_no_gl_error("gray ramp draw");

        const auto pixels = read_rgb_row();
        std::cout << label << '=';
        for (int index = 0; index < kRampWidth; ++index)
        {
            const auto& pixel = pixels[static_cast<std::size_t>(index)];
            require_near(pixel[0], expected[static_cast<std::size_t>(index)], label);
            require_near(pixel[1], expected[static_cast<std::size_t>(index)], label);
            require_near(pixel[2], expected[static_cast<std::size_t>(index)], label);
            std::cout << static_cast<int>(pixel[0]) << (index + 1 == kRampWidth ? '\n' : ',');
        }
    }

    void verify_straight_alpha_blend(const GlProgram& program)
    {
        glViewport(0, 0, 1, 1);
        glClearColor(
            static_cast<float>(kBlendDestination[0]) / 255.0F,
            static_cast<float>(kBlendDestination[1]) / 255.0F,
            static_cast<float>(kBlendDestination[2]) / 255.0F,
            1.0F
        );
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        program.use();
        draw_fullscreen_triangle();
        require_no_gl_error("straight-alpha blend draw");

        std::array<std::uint8_t, 4> pixel{};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        require_no_gl_error("straight-alpha glReadPixels");
        for (std::size_t channel = 0; channel < kExpectedBlend.size(); ++channel)
        {
            require_near(pixel[channel], kExpectedBlend[channel], "straight-alpha blend");
        }
        std::cout << "straight_alpha_blend=" << static_cast<int>(pixel[0]) << ',' << static_cast<int>(pixel[1]) << ','
                  << static_cast<int>(pixel[2]) << ',' << static_cast<int>(pixel[3]) << '\n';
        glDisable(GL_BLEND);
    }

    void verify_srgb_texture_decode(const GlProgram& program)
    {
        constexpr int kWidth = 3;
        const std::array<std::uint8_t, kWidth * 4> pixels{
            0,
            0,
            0,
            255,
            127,
            127,
            127,
            128,
            255,
            255,
            255,
            255,
        };
        RelayCore::render::Texture2D texture{};
        texture.initialize_rgba(kWidth, 1, pixels, RelayCore::render::TextureEncoding::srgb);
        texture.bind(0);
        GLint internal_format = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
        if (internal_format != GL_SRGB8_ALPHA8)
        {
            throw std::runtime_error("Texture2D sRGB upload did not use GL_SRGB8_ALPHA8");
        }
        std::cout << "srgb_texture_internal_format=GL_SRGB8_ALPHA8\n";

        GlTextureAttachment attachment{kWidth, 1, GL_RGBA16F, GL_FLOAT};
        GlFramebuffer framebuffer{attachment};
        framebuffer.bind();
        glDisable(GL_FRAMEBUFFER_SRGB);
        glViewport(0, 0, kWidth, 1);
        program.use();
        texture.bind(0);
        glUniform1i(program.uniform("u_srgb_texture"), 0);
        draw_fullscreen_triangle();
        require_no_gl_error("sRGB texture decode draw");

        std::array<std::array<float, 4>, kWidth> decoded{};
        glReadPixels(0, 0, kWidth, 1, GL_RGBA, GL_FLOAT, decoded.data());
        require_no_gl_error("sRGB texture decode glReadPixels");
        const std::array<float, kWidth> expected{0.0F, srgb_to_linear(127.0F / 255.0F), 1.0F};
        for (int index = 0; index < kWidth; ++index)
        {
            require_near(
                decoded[static_cast<std::size_t>(index)][0],
                expected[static_cast<std::size_t>(index)],
                "sRGB texture decode"
            );
        }
        require_near(decoded[1][3], 128.0F / 255.0F, "sRGB texture alpha");
        std::cout << "srgb_decode_0=" << decoded[0][0] << '\n';
        std::cout << "srgb_decode_127=" << decoded[1][0] << '\n';
        std::cout << "srgb_decode_255=" << decoded[2][0] << '\n';
        std::cout << "srgb_decode_alpha_128=" << decoded[1][3] << '\n';
        std::cout << "srgb_decode_baseline=passed\n";
    }

    [[nodiscard]] auto
    draw_srgb_encode_and_read(const GlFramebuffer& framebuffer, const GlProgram& program, const bool enabled)
        -> std::array<std::array<std::uint8_t, 4>, 4>
    {
        framebuffer.bind();
        if (enabled)
        {
            glEnable(GL_FRAMEBUFFER_SRGB);
        } else
        {
            glDisable(GL_FRAMEBUFFER_SRGB);
        }
        glViewport(0, 0, 4, 1);
        glDisable(GL_BLEND);
        program.use();
        draw_fullscreen_triangle();
        require_no_gl_error("sRGB framebuffer encode draw");
        std::array<std::array<std::uint8_t, 4>, 4> pixels{};
        glReadPixels(0, 0, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        require_no_gl_error("sRGB framebuffer encode glReadPixels");
        return pixels;
    }

    void verify_srgb_framebuffer_encode(const GlProgram& program)
    {
        GlTextureAttachment attachment{4, 1, GL_SRGB8_ALPHA8, GL_UNSIGNED_BYTE};
        GlFramebuffer framebuffer{attachment};
        const auto enabled = draw_srgb_encode_and_read(framebuffer, program, true);
        const auto disabled = draw_srgb_encode_and_read(framebuffer, program, false);
        constexpr std::array<float, 4> kLinear{0.0F, 0.18F, 0.5F, 1.0F};
        for (int index = 0; index < 4; ++index)
        {
            const int expected_enabled = linear_to_srgb_byte(kLinear[static_cast<std::size_t>(index)]);
            const int expected_disabled =
                static_cast<int>(std::lround(kLinear[static_cast<std::size_t>(index)] * 255.0F));
            require_srgb_near(
                enabled[static_cast<std::size_t>(index)][0],
                expected_enabled,
                "sRGB framebuffer enabled"
            );
            require_srgb_near(
                disabled[static_cast<std::size_t>(index)][0],
                expected_disabled,
                "sRGB framebuffer disabled"
            );
            require_srgb_near(enabled[static_cast<std::size_t>(index)][3], 128, "sRGB framebuffer alpha");
            require_srgb_near(disabled[static_cast<std::size_t>(index)][3], 128, "raw framebuffer alpha");
        }
        for (const int index : {1, 2})
        {
            const int double_encoded = linear_to_srgb_byte(
                static_cast<float>(linear_to_srgb_byte(kLinear[static_cast<std::size_t>(index)])) / 255.0F
            );
            if (std::abs(static_cast<int>(enabled[static_cast<std::size_t>(index)][0]) - double_encoded)
                    <= kSrgbByteTolerance
                || enabled[static_cast<std::size_t>(index)][0] == disabled[static_cast<std::size_t>(index)][0])
            {
                throw std::runtime_error("sRGB framebuffer double-conversion guard failed");
            }
        }
        std::cout << "srgb_encode_enabled=" << static_cast<int>(enabled[0][0]) << ',' << static_cast<int>(enabled[1][0])
                  << ',' << static_cast<int>(enabled[2][0]) << ',' << static_cast<int>(enabled[3][0]) << '\n';
        std::cout << "srgb_encode_disabled=" << static_cast<int>(disabled[0][0]) << ','
                  << static_cast<int>(disabled[1][0]) << ',' << static_cast<int>(disabled[2][0]) << ','
                  << static_cast<int>(disabled[3][0]) << '\n';
        std::cout << "srgb_encode_alpha=" << static_cast<int>(enabled[1][3]) << '\n';
        std::cout << "srgb_encode_baseline=passed\n";
    }

    void verify_linear_straight_alpha_blend_in_srgb_framebuffer(const GlProgram& program)
    {
        GlTextureAttachment attachment{1, 1, GL_SRGB8_ALPHA8, GL_UNSIGNED_BYTE};
        GlFramebuffer framebuffer{attachment};
        framebuffer.bind();
        glViewport(0, 0, 1, 1);
        glEnable(GL_FRAMEBUFFER_SRGB);
        if (glIsEnabled(GL_FRAMEBUFFER_SRGB) != GL_TRUE)
        {
            throw std::runtime_error("sRGB framebuffer blend requires GL_FRAMEBUFFER_SRGB enabled");
        }

        constexpr std::array<float, 3> kLinearDestination{0.25F, 0.50F, 0.75F};
        constexpr std::array<float, 3> kLinearSource{0.80F, 0.20F, 0.40F};
        constexpr float kSourceAlpha = 0.50F;
        glClearColor(kLinearDestination[0], kLinearDestination[1], kLinearDestination[2], 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        program.use();
        draw_fullscreen_triangle();
        require_no_gl_error("linear straight-alpha sRGB blend draw");

        std::array<std::uint8_t, 4> pixel{};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        require_no_gl_error("linear straight-alpha sRGB blend readback");
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const float expected_linear =
                kLinearSource[channel] * kSourceAlpha + kLinearDestination[channel] * (1.0F - kSourceAlpha);
            require_srgb_near(pixel[channel], linear_to_srgb_byte(expected_linear), "linear straight-alpha sRGB blend");
        }
        require_srgb_near(pixel[3], 191, "linear straight-alpha alpha remains unencoded");
        std::cout << "linear_straight_alpha_srgb_blend=" << static_cast<int>(pixel[0]) << ','
                  << static_cast<int>(pixel[1]) << ',' << static_cast<int>(pixel[2]) << ','
                  << static_cast<int>(pixel[3]) << '\n';
        glDisable(GL_BLEND);
        glDisable(GL_FRAMEBUFFER_SRGB);
        if (glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE)
        {
            throw std::runtime_error("sRGB framebuffer blend requires GL_FRAMEBUFFER_SRGB disabled on exit");
        }
    }
} // namespace

int main()
{
    try
    {
        SdlVideoLifetime sdl{};
        HiddenGlContext context{};
        std::cout << "graphics_color_pipeline_baseline=context_ready\n";
        configure_default_framebuffer_path();
        const FramebufferBits framebuffer_bits = print_default_framebuffer_inventory();
        if (framebuffer_bits.red < 8 || framebuffer_bits.green < 8 || framebuffer_bits.blue < 8)
        {
            throw BaselineUnavailable(
                "default framebuffer requires RGB bit depth >= 8, got " + std::to_string(framebuffer_bits.red) + ','
                + std::to_string(framebuffer_bits.green) + ',' + std::to_string(framebuffer_bits.blue)
            );
        }
        std::cout << "graphics_color_pipeline_baseline=measurement_started\n";

        GlVertexArray vertex_array{};

        const std::array<std::uint8_t, kRampWidth * 4> pixels{
            0, 0, 0, 255, 63, 63, 63, 255, 127, 127, 127, 255, 191, 191, 191, 255, 255, 255, 255, 255,
        };
        RelayCore::render::Texture2D reference_texture{};
        reference_texture.initialize_rgba(kRampWidth, 1, pixels, RelayCore::render::TextureEncoding::linear);
        reference_texture.bind(0);
        GLint internal_format = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
        if (internal_format != GL_RGBA8)
        {
            throw std::runtime_error("Texture2D baseline upload did not use GL_RGBA8");
        }
        std::cout << "reference_texture_internal_format=GL_RGBA8\n";

        constexpr char kVertexShader[] = R"(#version 460 core
void main()
{
    const vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
})";
        constexpr char kRampFragmentShader[] = R"(#version 460 core
layout (location = 0) out vec4 out_color;
uniform sampler2D u_reference_texture;
uniform float u_multiplier;
void main()
{
    int sample_index = int(gl_FragCoord.x);
    vec3 stored_rgb = texelFetch(u_reference_texture, ivec2(sample_index, 0), 0).rgb;
    out_color = vec4(stored_rgb * u_multiplier, 1.0);
})";
        constexpr char kBlendFragmentShader[] = R"(#version 460 core
layout (location = 0) out vec4 out_color;
void main()
{
    out_color = vec4(0.8, 0.2, 0.4, 0.5);
})";
        constexpr char kSrgbDecodeFragmentShader[] = R"(#version 460 core
layout (location = 0) out vec4 out_color;
uniform sampler2D u_srgb_texture;
void main()
{
    out_color = texelFetch(u_srgb_texture, ivec2(int(gl_FragCoord.x), 0), 0);
})";
        constexpr char kSrgbEncodeFragmentShader[] = R"(#version 460 core
layout (location = 0) out vec4 out_color;
void main()
{
    const float linear_values[4] = float[4](0.0, 0.18, 0.5, 1.0);
    out_color = vec4(linear_values[int(gl_FragCoord.x)], 0.0, 0.0, 0.5);
})";

        const GlProgram ramp_program{kVertexShader, kRampFragmentShader};
        const GlProgram blend_program{kVertexShader, kBlendFragmentShader};
        const GlProgram srgb_decode_program{kVertexShader, kSrgbDecodeFragmentShader};
        const GlProgram srgb_encode_program{kVertexShader, kSrgbEncodeFragmentShader};
        verify_gray_ramp(reference_texture, ramp_program, 1.0F, kExpectedFullLight, "gray_ramp_multiplier_1_0");
        verify_gray_ramp(reference_texture, ramp_program, 0.5F, kExpectedHalfLight, "gray_ramp_multiplier_0_5");
        verify_straight_alpha_blend(blend_program);
        verify_srgb_texture_decode(srgb_decode_program);
        verify_srgb_framebuffer_encode(srgb_encode_program);
        verify_linear_straight_alpha_blend_in_srgb_framebuffer(blend_program);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_FRAMEBUFFER_SRGB);
        std::cout << "graphics_color_pipeline_baseline=passed\n";
        return 0;
    } catch (const BaselineUnavailable& error)
    {
        std::cerr << "graphics_color_pipeline_baseline=skipped: " << error.what() << '\n';
        return kSkippedExitCode;
    } catch (const std::exception& error)
    {
        std::cerr << "graphics_color_pipeline_baseline=failed: " << error.what() << '\n';
        return 1;
    }
}
