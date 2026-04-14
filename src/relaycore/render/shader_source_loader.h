/**
 * @file src/relaycore/render/shader_source_loader.h
 * @brief Loads GLSL source files with simple local #include expansion.
 */

#pragma once

#include <filesystem>
#include <string>

namespace RelayCore::render
{
    /**
     * @brief Loads a GLSL source file and expands local `#include "..."` directives.
     * @param path Root shader source file.
     * @param injected_source_after_version Optional source inserted after the root `#version` line.
     * @return Expanded shader source.
     */
    [[nodiscard]] auto
    load_shader_source(const std::filesystem::path& path, std::string_view injected_source_after_version = {})
        -> std::string;
} // namespace RelayCore::render
