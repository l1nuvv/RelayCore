/**
 * @file src/relaygame/main.cpp
 * @brief Executable entry point for the demo application.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "relaycore/app/run_application.h"
#include "relaygame/demo_game.h"

/**
 * @brief Starts the RelayCore demo application.
 * @param argc Process argument count.
 * @param argv Process argument vector.
 * @return Process exit code returned by the application runner.
 */
int main(int argc, char* argv[])
{
    return RelayCore::run_application(RelayGame::make_demo_game(), argc, argv);
}
