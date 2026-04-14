/**
 * @file src/relaycore/render/shader_source_loader.cpp
 * @brief GLSL source loading with local include expansion.
 */

#include "relaycore/render/shader_source_loader.h"

#include "relaycore/core/profiling.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace RelayCore::render
{
    namespace
    {
        [[nodiscard]] auto trim_left(std::string_view value) -> std::string_view
        {
            const std::size_t first_non_space = value.find_first_not_of(" \t");
            if (first_non_space == std::string_view::npos)
            {
                return {};
            }

            return value.substr(first_non_space);
        }

        [[nodiscard]] auto trim(std::string_view value) -> std::string_view
        {
            value = trim_left(value);
            const std::size_t last_non_space = value.find_last_not_of(" \t\r");
            if (last_non_space == std::string_view::npos)
            {
                return {};
            }

            return value.substr(0, last_non_space + 1);
        }

        [[nodiscard]] auto read_text_file(const std::filesystem::path& path) -> std::string
        {
            std::ifstream input(path, std::ios::in | std::ios::binary);
            if (!input)
            {
                throw std::runtime_error("Failed to open file: " + path.string());
            }

            std::ostringstream buffer;
            buffer << input.rdbuf();
            return buffer.str();
        }

        [[nodiscard]] auto parse_include_path(const std::string_view line) -> std::string
        {
            const std::string_view trimmed = trim_left(line);
            constexpr std::string_view kPrefix = "#include";
            if (!trimmed.starts_with(kPrefix))
            {
                return {};
            }

            const std::string_view remainder = trim(trimmed.substr(kPrefix.size()));
            const std::size_t first_quote = remainder.find('"');
            if (first_quote == std::string_view::npos)
            {
                throw std::runtime_error("Malformed shader include directive: " + std::string(line));
            }

            const std::size_t second_quote = remainder.find('"', first_quote + 1);
            if (second_quote == std::string_view::npos || second_quote == first_quote + 1)
            {
                throw std::runtime_error("Malformed shader include directive: " + std::string(line));
            }

            return std::string(remainder.substr(first_quote + 1, second_quote - first_quote - 1));
        }

        [[nodiscard]] auto
        expand_shader_source(const std::filesystem::path& path, std::vector<std::filesystem::path>& include_stack)
            -> std::string
        {
            RELAYCORE_PROFILE_ZONE_N("ShaderSourceLoader::expand_shader_source");
            const std::filesystem::path normalized_path = std::filesystem::weakly_canonical(path);
            for (const std::filesystem::path& stack_path : include_stack)
            {
                if (stack_path == normalized_path)
                {
                    std::ostringstream message;
                    message << "Shader include cycle detected at '" << normalized_path.string() << "'";
                    throw std::runtime_error(message.str());
                }
            }

            include_stack.push_back(normalized_path);
            const std::string source = read_text_file(normalized_path);
            std::ostringstream expanded;
            std::istringstream input(source);
            std::string line;
            while (std::getline(input, line))
            {
                const std::string include_path = parse_include_path(line);
                if (!include_path.empty())
                {
                    const std::filesystem::path child_path = normalized_path.parent_path() / include_path;
                    expanded << "// begin include: " << child_path.generic_string() << "\n";
                    expanded << expand_shader_source(child_path, include_stack);
                    expanded << "\n// end include: " << child_path.generic_string();
                } else
                {
                    expanded << line;
                }

                if (!input.eof())
                {
                    expanded << '\n';
                }
            }

            include_stack.pop_back();
            return expanded.str();
        }

        [[nodiscard]] auto inject_source_after_version(
            const std::string_view expanded_source,
            const std::string_view injected_source_after_version
        ) -> std::string
        {
            if (injected_source_after_version.empty())
            {
                return std::string{expanded_source};
            }

            const std::size_t version_end = expanded_source.find('\n');
            const std::string_view first_line =
                version_end == std::string_view::npos ? expanded_source : expanded_source.substr(0, version_end);

            if (trim_left(first_line).starts_with("#version"))
            {
                std::string result{};
                result.reserve(expanded_source.size() + injected_source_after_version.size() + 2);
                result.append(first_line);
                result.push_back('\n');
                result.append(injected_source_after_version);
                if (!result.empty() && result.back() != '\n')
                {
                    result.push_back('\n');
                }
                if (version_end != std::string_view::npos && (version_end + 1) < expanded_source.size())
                {
                    result.append(expanded_source.substr(version_end + 1));
                }
                return result;
            }

            std::string result{};
            result.reserve(expanded_source.size() + injected_source_after_version.size() + 1);
            result.append(injected_source_after_version);
            if (!result.empty() && result.back() != '\n')
            {
                result.push_back('\n');
            }
            result.append(expanded_source);
            return result;
        }
    } // namespace

    auto load_shader_source(const std::filesystem::path& path, const std::string_view injected_source_after_version)
        -> std::string
    {
        RELAYCORE_PROFILE_FUNCTION();
        std::vector<std::filesystem::path> include_stack;
        include_stack.reserve(8);
        return inject_source_after_version(expand_shader_source(path, include_stack), injected_source_after_version);
    }
} // namespace RelayCore::render
