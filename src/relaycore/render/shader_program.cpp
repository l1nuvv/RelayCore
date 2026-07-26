/**
 * @file src/relaycore/render/shader_program.cpp
 * @brief ShaderProgram implementation.
 */

#include "relaycore/render/shader_program.h"

#include "relaycore/core/profiling.h"
#include "relaycore/render/shader_source_loader.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace RelayCore::render
{
    namespace
    {
        class UniqueShader {
        public:
            UniqueShader() = default;

            explicit UniqueShader(const GLuint shader) noexcept : shader_(shader) {}

            ~UniqueShader() noexcept
            {
                reset();
            }

            UniqueShader(const UniqueShader&) = delete;
            auto operator=(const UniqueShader&) -> UniqueShader& = delete;

            UniqueShader(UniqueShader&& other) noexcept : shader_(std::exchange(other.shader_, 0)) {}

            auto operator=(UniqueShader&& other) noexcept -> UniqueShader&
            {
                if (this != &other)
                {
                    reset();
                    shader_ = std::exchange(other.shader_, 0);
                }

                return *this;
            }

            [[nodiscard]] auto get() const noexcept -> GLuint
            {
                return shader_;
            }

            auto release() noexcept -> GLuint
            {
                return std::exchange(shader_, 0);
            }

            void reset() noexcept
            {
                if (shader_ != 0)
                {
                    glDeleteShader(shader_);
                    shader_ = 0;
                }
            }

        private:
            GLuint shader_ = 0;
        };

        class UniqueProgram {
        public:
            UniqueProgram() = default;

            explicit UniqueProgram(const GLuint program) noexcept : program_(program) {}

            ~UniqueProgram() noexcept
            {
                reset();
            }

            UniqueProgram(const UniqueProgram&) = delete;
            auto operator=(const UniqueProgram&) -> UniqueProgram& = delete;

            UniqueProgram(UniqueProgram&& other) noexcept : program_(std::exchange(other.program_, 0)) {}

            auto operator=(UniqueProgram&& other) noexcept -> UniqueProgram&
            {
                if (this != &other)
                {
                    reset();
                    program_ = std::exchange(other.program_, 0);
                }

                return *this;
            }

            [[nodiscard]] auto get() const noexcept -> GLuint
            {
                return program_;
            }

            auto release() noexcept -> GLuint
            {
                return std::exchange(program_, 0);
            }

            void reset() noexcept
            {
                if (program_ != 0)
                {
                    glDeleteProgram(program_);
                    program_ = 0;
                }
            }

        private:
            GLuint program_ = 0;
        };

        /**
         * @brief Builds a shader-compilation error message.
         * @param source_path Path to the shader source file.
         * @param stage Human-readable shader stage name.
         * @param info_log Compiler output from OpenGL.
         * @return Formatted exception message.
         */
        auto build_shader_error_message(
            const std::filesystem::path& source_path,
            const std::string& stage,
            const std::string& info_log
        ) -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            std::ostringstream stream;
            stream << "Failed to compile " << stage << " shader '" << source_path.string() << "':\n" << info_log;
            return stream.str();
        }

        /**
         * @brief Builds a shader-linking error message.
         * @param vertex_shader_path Path to the vertex shader source.
         * @param fragment_shader_path Path to the fragment shader source.
         * @param info_log Linker output from OpenGL.
         * @return Formatted exception message.
         */
        auto build_program_error_message(
            const std::filesystem::path& vertex_shader_path,
            const std::filesystem::path& fragment_shader_path,
            const std::string& info_log
        ) -> std::string
        {
            RELAYCORE_PROFILE_FUNCTION();
            std::ostringstream stream;
            stream << "Failed to link shader program ('" << vertex_shader_path.string() << "', '"
                   << fragment_shader_path.string() << "'):\n"
                   << info_log;
            return stream.str();
        }
    } // namespace

    ShaderProgram::~ShaderProgram() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        reset();
    }

    ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept :
        program_id_(std::exchange(other.program_id_, 0)),
        uniform_location_cache_(std::move(other.uniform_location_cache_))
    {
        RELAYCORE_PROFILE_FUNCTION();
    }

    auto ShaderProgram::operator=(ShaderProgram&& other) noexcept -> ShaderProgram&
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (this != &other)
        {
            reset();
            program_id_ = std::exchange(other.program_id_, 0);
            uniform_location_cache_ = std::move(other.uniform_location_cache_);
        }

        return *this;
    }

    void ShaderProgram::load_from_files(
        const std::filesystem::path& vertex_shader_path,
        const std::filesystem::path& fragment_shader_path,
        const ShaderFileSourcePatches& source_patches
    )
    {
        RELAYCORE_PROFILE_FUNCTION();
        reset();

        const std::string vertex_source = load_shader_source(vertex_shader_path, source_patches.vertex_injected_source);
        const std::string fragment_source =
            load_shader_source(fragment_shader_path, source_patches.fragment_injected_source);

        UniqueShader vertex_shader{compile_shader(GL_VERTEX_SHADER, vertex_source, vertex_shader_path)};
        UniqueShader fragment_shader{compile_shader(GL_FRAGMENT_SHADER, fragment_source, fragment_shader_path)};

        UniqueProgram program{glCreateProgram()};
        if (program.get() == 0)
        {
            throw std::runtime_error("glCreateProgram returned 0");
        }

        glAttachShader(program.get(), vertex_shader.get());
        glAttachShader(program.get(), fragment_shader.get());
        glBindAttribLocation(program.get(), 0, "in_position");
        glBindAttribLocation(program.get(), 1, "in_color");
        glLinkProgram(program.get());

        GLint link_status = GL_FALSE;
        glGetProgramiv(program.get(), GL_LINK_STATUS, &link_status);
        if (link_status != GL_TRUE)
        {
            GLint log_length = 0;
            glGetProgramiv(program.get(), GL_INFO_LOG_LENGTH, &log_length);

            std::string info_log(static_cast<std::size_t>(log_length > 1 ? log_length : 1), '\0');
            glGetProgramInfoLog(program.get(), log_length, nullptr, info_log.data());

            throw std::runtime_error(build_program_error_message(vertex_shader_path, fragment_shader_path, info_log));
        }

        program_id_ = program.release();
        uniform_location_cache_.clear();
    }

    void ShaderProgram::bind() const
    {
        RELAYCORE_PROFILE_ZONE_N("ShaderProgram::bind");
        glUseProgram(program_id_);
    }

    void ShaderProgram::set_mat4(const std::string_view name, const glm::mat4& value) const
    {
        RELAYCORE_PROFILE_ZONE_N("ShaderProgram::set_mat4_named");
        set_mat4(uniform_location(name), value);
    }

    void ShaderProgram::set_mat4(const int location, const glm::mat4& value) const
    {
        RELAYCORE_PROFILE_ZONE_N("ShaderProgram::set_mat4");
        if (location >= 0)
        {
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
        }
    }

    void ShaderProgram::set_vec3(const int location, const glm::vec3& value) const
    {
        RELAYCORE_PROFILE_ZONE_N("ShaderProgram::set_vec3");
        if (location >= 0)
        {
            glUniform3fv(location, 1, glm::value_ptr(value));
        }
    }

    void ShaderProgram::set_float(const int location, const float value) const
    {
        RELAYCORE_PROFILE_ZONE_N("ShaderProgram::set_float");
        if (location >= 0)
        {
            glUniform1f(location, value);
        }
    }

    void ShaderProgram::set_int(const int location, const int value) const
    {
        RELAYCORE_PROFILE_ZONE_N("ShaderProgram::set_int");
        if (location >= 0)
        {
            glUniform1i(location, value);
        }
    }

    void ShaderProgram::reset() noexcept
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (program_id_ != 0)
        {
            glDeleteProgram(program_id_);
            program_id_ = 0;
        }

        uniform_location_cache_.clear();
    }

    auto ShaderProgram::uniform_location(const std::string_view name) const -> int
    {
        RELAYCORE_PROFILE_FUNCTION();
        const std::string key(name);
        if (const auto cached = uniform_location_cache_.find(key); cached != uniform_location_cache_.end())
        {
            return cached->second;
        }

        const int location = glGetUniformLocation(program_id_, key.c_str());
        uniform_location_cache_.emplace(key, location);
        return location;
    }

    auto ShaderProgram::compile_shader(
        const unsigned int shader_type,
        const std::string& source,
        const std::filesystem::path& source_path
    ) -> unsigned int
    {
        RELAYCORE_PROFILE_FUNCTION();
        UniqueShader shader{glCreateShader(shader_type)};
        if (shader.get() == 0)
        {
            throw std::runtime_error("glCreateShader returned 0");
        }

        const char* source_ptr = source.c_str();
        glShaderSource(shader.get(), 1, &source_ptr, nullptr);
        glCompileShader(shader.get());

        GLint compile_status = GL_FALSE;
        glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &compile_status);
        if (compile_status != GL_TRUE)
        {
            GLint log_length = 0;
            glGetShaderiv(shader.get(), GL_INFO_LOG_LENGTH, &log_length);

            std::string info_log(static_cast<std::size_t>(log_length > 1 ? log_length : 1), '\0');
            glGetShaderInfoLog(shader.get(), log_length, nullptr, info_log.data());

            const std::string stage = shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment";
            throw std::runtime_error(build_shader_error_message(source_path, stage, info_log));
        }

        return shader.release();
    }
} // namespace RelayCore::render
