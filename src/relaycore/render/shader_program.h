/**
 * @file src/relaycore/render/shader_program.h
 * @brief OpenGL shader-program wrapper with cached uniform lookups.
 */

#pragma once

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

namespace RelayCore::render
{
    /**
     * @brief Owns an OpenGL shader program and its cached uniform locations.
     */
    class ShaderProgram {
    public:
        /**
         * @brief Creates an empty shader-program wrapper.
         */
        ShaderProgram() = default;

        /**
         * @brief Releases the owned OpenGL program.
         */
        ~ShaderProgram() noexcept;

        ShaderProgram(const ShaderProgram&) = delete; ///< ShaderProgram is not copyable.

        /**
         * @brief Disables copy assignment.
         * @return Reference to this object.
         */
        auto operator=(const ShaderProgram&) -> ShaderProgram& = delete;

        /**
         * @brief Moves OpenGL program ownership into a new wrapper.
         * @param other Source wrapper.
         */
        ShaderProgram(ShaderProgram&& other) noexcept;

        /**
         * @brief Moves OpenGL program ownership into this wrapper.
         * @param other Source wrapper.
         * @return Reference to this object.
         */
        auto operator=(ShaderProgram&& other) noexcept -> ShaderProgram&;

        /**
         * @brief Loads, compiles, and links shaders from source files.
         * @param vertex_shader_path Path to the vertex shader source.
         * @param fragment_shader_path Path to the fragment shader source.
         */
        void load_from_files(
            const std::filesystem::path& vertex_shader_path,
            const std::filesystem::path& fragment_shader_path
        );

        /**
         * @brief Binds the OpenGL program for subsequent draw calls.
         */
        void bind() const;

        /**
         * @brief Sets a mat4 uniform by name.
         * @param name Uniform name.
         * @param value Matrix value to upload.
         */
        void set_mat4(std::string_view name, const glm::mat4& value) const;

        /**
         * @brief Sets a mat4 uniform by explicit location.
         * @param location Uniform location.
         * @param value Matrix value to upload.
         */
        void set_mat4(int location, const glm::mat4& value) const;

        /**
         * @brief Sets a vec3 uniform by explicit location.
         * @param location Uniform location.
         * @param value Vector value to upload.
         */
        void set_vec3(int location, const glm::vec3& value) const;

        /**
         * @brief Sets a float uniform by explicit location.
         * @param location Uniform location.
         * @param value Scalar value to upload.
         */
        void set_float(int location, float value) const;

    private:
        /**
         * @brief Releases the current OpenGL program and clears caches.
         */
        void reset() noexcept;

        /**
         * @brief Returns a cached or freshly queried uniform location.
         * @param name Uniform name.
         * @return Uniform location, or a negative value if not found.
         */
        [[nodiscard]] auto uniform_location(std::string_view name) const -> int;

        /**
         * @brief Reads a text file into memory.
         * @param path File to read.
         * @return Entire file contents.
         */
        [[nodiscard]] static auto read_text_file(const std::filesystem::path& path) -> std::string;

        /**
         * @brief Compiles a shader stage from source code.
         * @param shader_type OpenGL shader-stage enum.
         * @param source Shader source code.
         * @param source_path Path used for diagnostics.
         * @return Compiled shader object identifier.
         */
        [[nodiscard]] static auto
        compile_shader(unsigned int shader_type, const std::string& source, const std::filesystem::path& source_path)
            -> unsigned int;

        unsigned int program_id_ = 0;                                           ///< Owned OpenGL program identifier.
        mutable std::unordered_map<std::string, int> uniform_location_cache_{}; ///< Cached uniform lookups by name.
    };
} // namespace RelayCore::render
