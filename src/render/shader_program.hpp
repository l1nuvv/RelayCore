#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include <glm/mat4x4.hpp>

namespace RelayCore::render
{
    class ShaderProgram {
    public:
        ShaderProgram() = default;

        ~ShaderProgram() noexcept;

        ShaderProgram(const ShaderProgram&) = delete;

        auto operator=(const ShaderProgram&) -> ShaderProgram& = delete;

        ShaderProgram(ShaderProgram&& other) noexcept;

        auto operator=(ShaderProgram&& other) noexcept -> ShaderProgram&;

        void load_from_files(const std::filesystem::path& vertex_shader_path,
                             const std::filesystem::path& fragment_shader_path);

        void bind() const;

        void set_mat4(std::string_view name, const glm::mat4& value) const;

    private:
        void reset() noexcept;

        [[nodiscard]] auto uniform_location(std::string_view name) const -> int;

        [[nodiscard]] static auto read_text_file(const std::filesystem::path& path) -> std::string;

        [[nodiscard]] static auto compile_shader(unsigned int                 shader_type,
                                                 const std::string&           source,
                                                 const std::filesystem::path& source_path) -> unsigned int;

        unsigned int                                 program_id_ = 0;
        mutable std::unordered_map<std::string, int> uniform_location_cache_{};
    };
} // namespace RelayCore::render
