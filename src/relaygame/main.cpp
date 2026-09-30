#include "relaycore/app/run_application.h"
#include "relaycore/core/profiling.h"
#include "relaygame/demo_game.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[])
{
    RELAYCORE_PROFILE_FUNCTION();
    return RelayCore::run_application(RelayGame::make_demo_game(), argc, argv);
}
