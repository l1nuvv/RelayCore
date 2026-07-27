#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glad/gl.h>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    constexpr int kProbeWidth = 64;
    constexpr int kProbeHeight = 64;
    constexpr int kExpectedLinearByte = 128;
    constexpr int kExpectedSrgbByte = 188;
    constexpr int kByteTolerance = 3;

    enum class AttributeRequest {
        current_like,
        explicit_rgba8,
    };

    [[nodiscard]] auto request_name(const AttributeRequest request) noexcept -> const char*
    {
        return request == AttributeRequest::explicit_rgba8 ? "explicit_rgba8" : "current_like";
    }

    void require_sdl(const bool success, const std::string_view operation)
    {
        if (!success)
        {
            throw std::runtime_error(std::string{operation} + " failed: " + SDL_GetError());
        }
    }

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

    [[nodiscard]] auto gl_string(const GLenum name) -> const char*
    {
        const auto* value = reinterpret_cast<const char*>(glGetString(name));
        return value != nullptr ? value : "unavailable";
    }

    [[nodiscard]] auto encoding_name(const GLint encoding) noexcept -> const char*
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

    [[nodiscard]] auto enum_hex(const GLint value) -> std::string
    {
        constexpr char digits[] = "0123456789ABCDEF";
        unsigned int remaining = static_cast<unsigned int>(value);
        std::string result{"0x"};
        bool started = false;
        for (int shift = 28; shift >= 0; shift -= 4)
        {
            const unsigned int digit = (remaining >> static_cast<unsigned int>(shift)) & 0xFU;
            if (digit != 0U || started || shift == 0)
            {
                result.push_back(digits[digit]);
                started = true;
            }
        }
        return result;
    }

    class SdlVideoLifetime {
    public:
        SdlVideoLifetime()
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
            {
                throw std::runtime_error("SDL video initialization failed: " + std::string{SDL_GetError()});
            }
        }

        ~SdlVideoLifetime() noexcept
        {
            SDL_Quit();
        }

        SdlVideoLifetime(const SdlVideoLifetime&) = delete;
        auto operator=(const SdlVideoLifetime&) -> SdlVideoLifetime& = delete;
    };

    class ProbeContext {
    public:
        explicit ProbeContext(const AttributeRequest request) : request_(request)
        {
            SDL_GL_ResetAttributes();

            require_sdl(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4), "set GL major version");
            require_sdl(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6), "set GL minor version");
            require_sdl(
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE),
                "set GL core profile"
            );
            require_sdl(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1), "set double buffering");
            require_sdl(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24), "set depth size");
            require_sdl(SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8), "set stencil size");
            require_sdl(
                SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1),
                "request sRGB-capable default framebuffer"
            );

            if (request_ == AttributeRequest::explicit_rgba8)
            {
                require_sdl(SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8), "set red size");
                require_sdl(SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8), "set green size");
                require_sdl(SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8), "set blue size");
                require_sdl(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8), "set alpha size");
            }

            window_ = SDL_CreateWindow(
                request_name(request_),
                kProbeWidth,
                kProbeHeight,
                SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
            );
            if (window_ == nullptr)
            {
                throw std::runtime_error("hidden SDL OpenGL window creation failed: " + std::string{SDL_GetError()});
            }

            context_ = SDL_GL_CreateContext(window_);
            if (context_ == nullptr)
            {
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                throw std::runtime_error("OpenGL context creation failed: " + std::string{SDL_GetError()});
            }

            if (gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) == 0)
            {
                static_cast<void>(SDL_GL_DestroyContext(context_));
                context_ = nullptr;
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                throw std::runtime_error("gladLoadGL failed");
            }
        }

        ~ProbeContext() noexcept
        {
            if (context_ != nullptr)
            {
                static_cast<void>(SDL_GL_DestroyContext(context_));
            }
            if (window_ != nullptr)
            {
                SDL_DestroyWindow(window_);
            }
        }

        ProbeContext(const ProbeContext&) = delete;
        auto operator=(const ProbeContext&) -> ProbeContext& = delete;

        [[nodiscard]] auto request() const noexcept -> AttributeRequest
        {
            return request_;
        }

    private:
        AttributeRequest request_ = AttributeRequest::current_like;
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
                throw std::runtime_error("probe program creation failed");
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
                GLint log_length = 0;
                glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_length);
                std::string log(static_cast<std::size_t>(std::max(log_length, 1)), '\0');
                glGetProgramInfoLog(program_, log_length, nullptr, log.data());
                glDeleteProgram(program_);
                program_ = 0;
                throw std::runtime_error("probe shader link failed: " + log);
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

        GlProgram(const GlProgram&) = delete;
        auto operator=(const GlProgram&) -> GlProgram& = delete;

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
                GLint log_length = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
                std::string log(static_cast<std::size_t>(std::max(log_length, 1)), '\0');
                glGetShaderInfoLog(shader, log_length, nullptr, log.data());
                glDeleteShader(shader);
                throw std::runtime_error("probe shader compilation failed: " + log);
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

    struct ProbeResult {
        AttributeRequest request = AttributeRequest::current_like;
        int reported_srgb_capable = 0;
        std::array<int, 4> rgba_bits{};
        GLint framebuffer_encoding = GL_NONE;
        GLint draw_buffer = GL_NONE;
        GLint read_buffer = GL_NONE;
        std::array<std::uint8_t, 4> disabled{};
        std::array<std::uint8_t, 4> enabled{};
    };

    [[nodiscard]] auto query_sdl_attribute(const SDL_GLAttr attribute, const std::string_view label) -> int
    {
        int value = 0;
        if (!SDL_GL_GetAttribute(attribute, &value))
        {
            throw std::runtime_error(std::string{label} + " query failed: " + SDL_GetError());
        }
        return value;
    }

    [[nodiscard]] auto draw_linear_half(const GlProgram& program, const bool enable_srgb) -> std::array<std::uint8_t, 4>
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);
        glViewport(0, 0, 1, 1);
        glDisable(GL_BLEND);
        glDisable(GL_DITHER);

        if (enable_srgb)
        {
            glEnable(GL_FRAMEBUFFER_SRGB);
        } else
        {
            glDisable(GL_FRAMEBUFFER_SRGB);
        }

        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        program.use();
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glFinish();
        require_no_gl_error(enable_srgb ? "enabled default-framebuffer draw" : "disabled default-framebuffer draw");

        std::array<std::uint8_t, 4> pixel{};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        require_no_gl_error(
            enable_srgb ? "enabled default-framebuffer readback" : "disabled default-framebuffer readback"
        );
        return pixel;
    }

    [[nodiscard]] auto run_probe(const AttributeRequest request) -> ProbeResult
    {
        ProbeContext context{request};

        std::cout << "probe=" << request_name(request) << '\n';
        std::cout << "SDL_video_driver="
                  << (SDL_GetCurrentVideoDriver() != nullptr ? SDL_GetCurrentVideoDriver() : "unavailable") << '\n';
        std::cout << "GL_VERSION=" << gl_string(GL_VERSION) << '\n';
        std::cout << "GL_VENDOR=" << gl_string(GL_VENDOR) << '\n';
        std::cout << "GL_RENDERER=" << gl_string(GL_RENDERER) << '\n';
        std::cout << "SDL_requested_srgb_capability=1\n";

        ProbeResult result{};
        result.request = request;
        result.reported_srgb_capable =
            query_sdl_attribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, "SDL reported sRGB capability");
        result.rgba_bits = {
            query_sdl_attribute(SDL_GL_RED_SIZE, "red bits"),
            query_sdl_attribute(SDL_GL_GREEN_SIZE, "green bits"),
            query_sdl_attribute(SDL_GL_BLUE_SIZE, "blue bits"),
            query_sdl_attribute(SDL_GL_ALPHA_SIZE, "alpha bits"),
        };

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glReadBuffer(GL_BACK);

        clear_gl_errors();
        glGetFramebufferAttachmentParameteriv(
            GL_DRAW_FRAMEBUFFER,
            GL_BACK_LEFT,
            GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
            &result.framebuffer_encoding
        );
        require_no_gl_error("default framebuffer colour-encoding query");

        glGetIntegerv(GL_DRAW_BUFFER, &result.draw_buffer);
        glGetIntegerv(GL_READ_BUFFER, &result.read_buffer);
        require_no_gl_error("default framebuffer draw/read buffer query");

        std::cout << "SDL_reported_srgb_capability=" << result.reported_srgb_capable << '\n';
        std::cout << "default_framebuffer_bits=" << result.rgba_bits[0] << ',' << result.rgba_bits[1] << ','
                  << result.rgba_bits[2] << ',' << result.rgba_bits[3] << '\n';
        std::cout << "default_framebuffer_color_encoding=" << encoding_name(result.framebuffer_encoding) << '\n';
        std::cout << "GL_DRAW_BUFFER=" << enum_hex(result.draw_buffer) << '\n';
        std::cout << "GL_READ_BUFFER=" << enum_hex(result.read_buffer) << '\n';

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

        GlVertexArray vertex_array{};
        const GlProgram program{kVertexShader, kFragmentShader};

        result.disabled = draw_linear_half(program, false);
        result.enabled = draw_linear_half(program, true);

        std::cout << "framebuffer_srgb_disabled=" << static_cast<int>(result.disabled[0]) << ','
                  << static_cast<int>(result.disabled[1]) << ',' << static_cast<int>(result.disabled[2]) << ','
                  << static_cast<int>(result.disabled[3]) << '\n';
        std::cout << "framebuffer_srgb_enabled=" << static_cast<int>(result.enabled[0]) << ','
                  << static_cast<int>(result.enabled[1]) << ',' << static_cast<int>(result.enabled[2]) << ','
                  << static_cast<int>(result.enabled[3]) << '\n';

        glDisable(GL_FRAMEBUFFER_SRGB);
        glEnable(GL_DITHER);
        return result;
    }

    [[nodiscard]] auto near_byte(const std::uint8_t actual, const int expected) noexcept -> bool
    {
        return std::abs(static_cast<int>(actual) - expected) <= kByteTolerance;
    }

    [[nodiscard]] auto disabled_is_linear(const ProbeResult& result) noexcept -> bool
    {
        return near_byte(result.disabled[0], kExpectedLinearByte) && near_byte(result.disabled[1], kExpectedLinearByte)
               && near_byte(result.disabled[2], kExpectedLinearByte);
    }

    [[nodiscard]] auto enabled_is_linear(const ProbeResult& result) noexcept -> bool
    {
        return near_byte(result.enabled[0], kExpectedLinearByte) && near_byte(result.enabled[1], kExpectedLinearByte)
               && near_byte(result.enabled[2], kExpectedLinearByte);
    }

    [[nodiscard]] auto enabled_is_srgb(const ProbeResult& result) noexcept -> bool
    {
        return near_byte(result.enabled[0], kExpectedSrgbByte) && near_byte(result.enabled[1], kExpectedSrgbByte)
               && near_byte(result.enabled[2], kExpectedSrgbByte);
    }

    [[nodiscard]] auto behaves_as_srgb(const ProbeResult& result) noexcept -> bool
    {
        return disabled_is_linear(result) && enabled_is_srgb(result);
    }

    [[nodiscard]] auto behaves_as_linear(const ProbeResult& result) noexcept -> bool
    {
        return disabled_is_linear(result) && enabled_is_linear(result);
    }
} // namespace

int main()
{
    try
    {
        SdlVideoLifetime sdl{};

        std::optional<ProbeResult> current_like{};
        std::optional<ProbeResult> explicit_rgba8{};

        try
        {
            current_like = run_probe(AttributeRequest::current_like);
        } catch (const std::exception& error)
        {
            std::cerr << "probe=current_like unavailable: " << error.what() << '\n';
        }

        try
        {
            explicit_rgba8 = run_probe(AttributeRequest::explicit_rgba8);
        } catch (const std::exception& error)
        {
            std::cerr << "probe=explicit_rgba8 unavailable: " << error.what() << '\n';
        }

        if (!current_like.has_value() && !explicit_rgba8.has_value())
        {
            std::cerr << "default_framebuffer_srgb_probe=skipped: no probe context could be created\n";
            return 77;
        }

        if (current_like.has_value())
        {
            const ProbeResult& current = *current_like;

            if (current.framebuffer_encoding == GL_LINEAR && behaves_as_srgb(current))
            {
                std::cout << "outcome=B\n";
                std::cout << "conclusion=encoding query reports GL_LINEAR but behavioral sRGB encoding works\n";
                return 0;
            }

            if (current.framebuffer_encoding == GL_SRGB && behaves_as_srgb(current))
            {
                std::cout << "outcome=current_request_srgb_available\n";
                std::cout << "conclusion=current-like request produced a verified sRGB default framebuffer\n";
                return 0;
            }

            if (current.framebuffer_encoding == GL_LINEAR && behaves_as_linear(current))
            {
                if (explicit_rgba8.has_value() && explicit_rgba8->framebuffer_encoding == GL_SRGB
                    && behaves_as_srgb(*explicit_rgba8))
                {
                    std::cout << "outcome=C\n";
                    std::cout << "conclusion=explicit RGBA8 request produces a verified sRGB default framebuffer\n";
                    return 0;
                }

                std::cout << "outcome=A\n";
                std::cout
                    << "conclusion=window default framebuffer remains linear; GL_FRAMEBUFFER_SRGB has no effect\n";
                return 0;
            }
        }

        if (explicit_rgba8.has_value() && explicit_rgba8->framebuffer_encoding == GL_SRGB
            && behaves_as_srgb(*explicit_rgba8))
        {
            std::cout << "outcome=C\n";
            std::cout << "conclusion=explicit RGBA8 request produces a verified sRGB default framebuffer\n";
            return 0;
        }

        std::cerr << "outcome=inconclusive\n";
        std::cerr << "conclusion=readback did not match the controlled linear or sRGB byte contracts\n";
        return 1;
    } catch (const std::exception& error)
    {
        std::cerr << "default_framebuffer_srgb_probe=skipped: " << error.what() << '\n';
        return 77;
    }
}
