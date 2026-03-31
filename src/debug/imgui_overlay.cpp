#include "debug/imgui_overlay.hpp"

#include "audio/audio_system.hpp"
#include "core/log.hpp"
#include "platform/window.hpp"
#include "render/camera.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

namespace RelayCore::debug
{
    ImGuiOverlay::ImGuiOverlay(const ImGuiOverlayConfig& config) :
        config_(config) {}

    ImGuiOverlay::~ImGuiOverlay() noexcept { shutdown(); }

    void ImGuiOverlay::initialize(const Window& window)
    {
        if (initialized_ || !config_.enabled) { return; }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io    = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForOpenGL(window.handle(), window.gl_context());
        ImGui_ImplOpenGL3_Init("#version 460 core");

        initialized_ = true;
        log::info("ImGui overlay initialized");
    }

    void ImGuiOverlay::shutdown() noexcept
    {
        if (!initialized_) { return; }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
    }

    void ImGuiOverlay::process_event(const SDL_Event& event)
    {
        if (!initialized_) { return; }

        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    void ImGuiOverlay::begin_frame()
    {
        if (!initialized_) { return; }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiOverlay::draw_diagnostics(
        const FrameInfo&                 frame,
        const render::PerspectiveCamera& camera,
        const AudioSystem&               audio_system)
    {
        if (!initialized_) { return; }

        ImGui::Begin("RelayCore Diagnostics");
        ImGui::Text("Frame time: %.3f ms", frame.delta_seconds * 1000.0);
        ImGui::Text("FPS: %.1f", frame.delta_seconds > 0.0 ? 1.0 / frame.delta_seconds : 0.0);

        const auto& position = camera.position();
        ImGui::SeparatorText("Camera");
        ImGui::Text("Position: %.2f %.2f %.2f", position.x, position.y, position.z);
        ImGui::Text("Yaw/Pitch: %.2f / %.2f", camera.yaw_radians(), camera.pitch_radians());

        ImGui::SeparatorText("Audio");
        ImGui::Text("Initialized: %s", audio_system.is_initialized() ? "yes" : "no");
        ImGui::Text("Master volume: %.2f", audio_system.config().master_volume);
        ImGui::End();

        if (config_.show_demo_window) { ImGui::ShowDemoWindow(&config_.show_demo_window); }
    }

    void ImGuiOverlay::render() const
    {
        if (!initialized_) { return; }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    auto ImGuiOverlay::is_initialized() const noexcept -> bool { return initialized_; }

    auto ImGuiOverlay::enabled() const noexcept -> bool { return config_.enabled; }
} // namespace RelayCore::debug
