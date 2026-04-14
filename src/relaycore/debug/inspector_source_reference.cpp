#include "relaycore/debug/inspector_source_reference.h"

#include "relaycore/core/profiling.h"

#include <filesystem>
#include <string>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#endif

namespace RelayCore::debug
{
    namespace
    {
        [[nodiscard]] auto make_source_location(const std::string& file_path, const int line, const int column)
            -> SourceLocation
        {
            return {
                .file_path = file_path,
                .line = line,
                .column = column,
            };
        }

#if defined(_WIN32)
        [[nodiscard]] auto utf8_to_wide(const std::string& value) -> std::wstring
        {
            if (value.empty())
            {
                return {};
            }

            const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
            if (length <= 1)
            {
                return {};
            }

            std::wstring wide(static_cast<std::size_t>(length), L'\0');
            (void) MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), length);
            wide.resize(static_cast<std::size_t>(length - 1));
            return wide;
        }

        [[nodiscard]] auto quote_argument(const std::wstring& argument) -> std::wstring
        {
            return L"\"" + argument + L"\"";
        }

        [[nodiscard]] auto
        try_shell_execute(const wchar_t* executable, const std::wstring& parameters, const char* strategy)
            -> SourceOpenResult
        {
            const HINSTANCE result = ShellExecuteW(
                nullptr,
                L"open",
                executable,
                parameters.empty() ? nullptr : parameters.c_str(),
                nullptr,
                SW_SHOWNORMAL
            );
            const auto code = reinterpret_cast<std::intptr_t>(result);
            if (code > 32)
            {
                return {
                    .launched = true,
                    .strategy = strategy,
                };
            }

            return {
                .launched = false,
                .strategy = strategy,
                .error = "ShellExecute failed with code " + std::to_string(code),
            };
        }
#endif
    } // namespace

    auto has_copyable_inspector_source(const render::InspectorDiagnostics& inspector) -> bool
    {
        RELAYCORE_PROFILE_FUNCTION();
        return !build_primary_inspector_source_reference(inspector).empty();
    }

    auto build_source_reference(const SourceLocation& location) -> std::string
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!location.valid())
        {
            return {};
        }

        return location.file_path + ":" + std::to_string(location.line) + ":" + std::to_string(location.column);
    }

    auto build_definition_inspector_source_location(const render::InspectorDiagnostics& inspector) -> SourceLocation
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!inspector.has_authored_source)
        {
            return {};
        }

        return make_source_location(inspector.authored_file_path, inspector.authored_line, inspector.authored_column);
    }

    auto build_placement_inspector_source_location(const render::InspectorDiagnostics& inspector) -> SourceLocation
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!inspector.has_authored_source)
        {
            return {};
        }

        return make_source_location(
            inspector.authored_placement_file_path,
            inspector.authored_placement_line,
            inspector.authored_placement_column
        );
    }

    auto build_primary_inspector_source_location(const render::InspectorDiagnostics& inspector) -> SourceLocation
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!inspector.has_authored_source)
        {
            return {};
        }

        const SourceLocation placement = build_placement_inspector_source_location(inspector);
        if (placement.valid())
        {
            return placement;
        }

        return build_definition_inspector_source_location(inspector);
    }

    auto build_primary_inspector_source_reference(const render::InspectorDiagnostics& inspector) -> std::string
    {
        RELAYCORE_PROFILE_FUNCTION();
        return build_source_reference(build_primary_inspector_source_location(inspector));
    }

    auto build_full_inspector_source_reference(const render::InspectorDiagnostics& inspector) -> std::string
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!inspector.has_authored_source)
        {
            return {};
        }

        const std::string definition = build_source_reference(build_definition_inspector_source_location(inspector));
        const std::string placement = build_source_reference(build_placement_inspector_source_location(inspector));

        if (!placement.empty() && !definition.empty())
        {
            return "placement " + placement + " | definition " + definition;
        }
        if (!placement.empty())
        {
            return placement;
        }
        return definition;
    }

    auto open_source_location(const SourceLocation& location) -> SourceOpenResult
    {
        RELAYCORE_PROFILE_FUNCTION();
        if (!location.valid())
        {
            return {
                .launched = false,
                .error = "No authored source location is available",
            };
        }

#if defined(_WIN32)
        const std::wstring wide_path = utf8_to_wide(location.file_path);
        if (wide_path.empty())
        {
            return {
                .launched = false,
                .error = "Failed to convert source path to a wide string",
            };
        }

        {
            const std::wstring parameters =
                L"--line " + std::to_wstring(location.line) + L" " + quote_argument(wide_path);
            if (const SourceOpenResult result = try_shell_execute(L"clion64.exe", parameters, "clion64.exe --line");
                result.launched)
            {
                return result;
            }
        }
        {
            const std::wstring parameters =
                L"--line " + std::to_wstring(location.line) + L" " + quote_argument(wide_path);
            if (const SourceOpenResult result = try_shell_execute(L"clion.exe", parameters, "clion.exe --line");
                result.launched)
            {
                return result;
            }
        }
        {
            const std::wstring parameters =
                L"-g "
                + quote_argument(
                    wide_path + L":" + std::to_wstring(location.line) + L":" + std::to_wstring(location.column)
                );
            if (const SourceOpenResult result = try_shell_execute(L"code.exe", parameters, "code.exe -g");
                result.launched)
            {
                return result;
            }
        }
        {
            const std::wstring parameters =
                L"-g "
                + quote_argument(
                    wide_path + L":" + std::to_wstring(location.line) + L":" + std::to_wstring(location.column)
                );
            if (const SourceOpenResult result =
                    try_shell_execute(L"code-insiders.exe", parameters, "code-insiders.exe -g");
                result.launched)
            {
                return result;
            }
        }

        const HINSTANCE fallback = ShellExecuteW(nullptr, L"open", wide_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        const auto fallback_code = reinterpret_cast<std::intptr_t>(fallback);
        if (fallback_code > 32)
        {
            return {
                .launched = true,
                .strategy = "shell open",
            };
        }

        return {
            .launched = false,
            .error =
                "No supported editor launcher was available for file:line navigation; ShellExecute failed with code "
                + std::to_string(fallback_code),
        };
#else
        return {
            .launched = false,
            .error = "Source navigation is only implemented for Windows in this build",
        };
#endif
    }
} // namespace RelayCore::debug
