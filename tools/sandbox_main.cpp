#include "relaycore/app/run_application.h"
#include "relaygame/demo_game.h"

#include <SDL3/SDL_main.h>
#include <memory>

int main(int argc, char** argv)
{
    return RelayCore::run_application(std::make_unique<RelayGame::DemoGame>(std::filesystem::path{}, true), argc, argv);
}
