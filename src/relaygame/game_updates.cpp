#include "relaygame/game_updates.h"

#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#endif

namespace RelayGame
{
    void signal_update_ready()
    {
#ifdef _WIN32
        static bool signaled = false;
        if (signaled)
            return;
        signaled = true;
        if (const char* event_name = SDL_getenv("RELAYCORE_UPDATE_READY_EVENT"))
        {
            if (const HANDLE event = OpenEventA(EVENT_MODIFY_STATE, FALSE, event_name))
            {
                SetEvent(event);
                CloseHandle(event);
            }
        }
#endif
    }

    auto can_update_game() -> bool
    {
        static const bool packaged = [] {
            const std::filesystem::path root{std::u8string_view{reinterpret_cast<const char8_t*>(SDL_GetBasePath())}};
            return std::filesystem::is_regular_file(root / "BUILD_MANIFEST.json")
                   && std::filesystem::is_regular_file(root / "RelayCoreUpdater.exe")
                   && std::filesystem::is_regular_file(root / "updates.json");
        }();
        return packaged;
    }

    auto check_game_update() -> UpdateRelease
    {
        const std::string root = SDL_GetBasePath();
        const std::string executable = root + "RelayCoreUpdater.exe";
        const char* args[]{executable.c_str(), "--check", root.c_str(), nullptr};
        const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process{
            SDL_CreateProcess(args, true),
            SDL_DestroyProcess
        };
        if (!process)
            throw std::runtime_error(SDL_GetError());
        std::size_t length = 0;
        int exit_code = 0;
        const std::unique_ptr<void, decltype(&SDL_free)> output{
            SDL_ReadProcess(process.get(), &length, &exit_code),
            SDL_free
        };
        if (!output)
            throw std::runtime_error("Не удалось получить ответ службы обновления.");
        const std::string text{static_cast<const char*>(output.get()), length};
        if (exit_code != 0)
            throw std::runtime_error(text);
        std::istringstream input{text};
        std::string state;
        UpdateRelease release;
        if (!std::getline(input, state) || !std::getline(input, release.version) || !(input >> release.bytes)
            || (state != "available" && state != "current"))
            throw std::runtime_error("Некорректный ответ службы обновления.");
        input.ignore();
        std::getline(input, release.notes, '\0');
        release.available = state == "available";
        return release;
    }

    void install_game_update(const std::string& version)
    {
        const std::string root = SDL_GetBasePath();
        const std::string executable = root + "RelayCoreUpdater.exe";
        const char* args[]{executable.c_str(), "--install", root.c_str(), version.c_str(), nullptr};
        const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process{
            SDL_CreateProcess(args, false),
            SDL_DestroyProcess
        };
        if (!process)
            throw std::runtime_error(SDL_GetError());
    }
} // namespace RelayGame
