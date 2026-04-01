#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "relaycore/app/run_application.hpp"
#include "relaygame/demo_game.hpp"

int main(int argc, char* argv[])
{
    return RelayCore::run_application(RelayGame::make_demo_game(), argc, argv);
}
