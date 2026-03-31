#pragma once

#include "app/frame_timer.hpp"

#include <SDL3/SDL.h>

namespace RelayCore
{
class AppDelegate
{
public:
    virtual ~AppDelegate() = default;

    virtual void on_startup() = 0;
    virtual void on_event(const SDL_Event& event) = 0;
    virtual void on_update(const FrameInfo& frame) = 0;
    virtual void on_shutdown() noexcept = 0;
};

class NullAppDelegate final : public AppDelegate
{
public:
    void on_startup() override {}
    void on_event(const SDL_Event& /*event*/) override {}
    void on_update(const FrameInfo& /*frame*/) override {}
    void on_shutdown() noexcept override {}
};
} // namespace RelayCore
