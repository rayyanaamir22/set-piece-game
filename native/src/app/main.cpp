// B1 platform shell: SDL3 window, GL 4.1 core, ImGui, fixed 240 Hz sim loop with render interpolation.
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cstdio>
#include <exception>

#include "app/overlay.h"
#include "platform/input.h"
#include "platform/window.h"
#include "sim/input_state.h"

namespace {
constexpr double SIM_DT = 1.0 / 240.0;  // same fixed step as the web build
constexpr double MAX_FRAME = 0.25;      // spiral-of-death guard
}  // namespace

int main() {
    try {
        platform::Window window("Set Piece", 1280, 720);
        platform::Input input;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL3_InitForOpenGL(window.handle(), window.context());
        ImGui_ImplOpenGL3_Init("#version 410 core");

        app::FrameStats stats;
        stats.glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        stats.glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        bool vsync = true;

        const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
        Uint64 last = SDL_GetPerformanceCounter();
        double acc = 0, smoothDt = 1.0 / 60.0;
        bool running = true;

        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                ImGui_ImplSDL3_ProcessEvent(&e);
                if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
                input.handleEvent(e, window.handle());
            }

            const Uint64 now = SDL_GetPerformanceCounter();
            const double frameDt = std::min(MAX_FRAME, static_cast<double>(now - last) / freq);
            last = now;
            smoothDt += (frameDt - smoothDt) * 0.05;

            const platform::Polled polled = input.poll(frameDt);

            // Fixed-step loop. Held input is identical on every tick of a frame; the mouse
            // delta is applied once, on the first tick (see porting contract, "Input edges").
            acc += frameDt;
            sim::InputState tickInput = polled.input;
            int steps = 0;
            while (acc >= SIM_DT) {
                // B2: sim::step(state, tickInput, SIM_DT);
                tickInput.aimDeltaX = tickInput.aimDeltaY = 0;
                ++stats.simTicks;
                ++steps;
                acc -= SIM_DT;
            }
            stats.stepsThisFrame = steps;
            stats.alpha = acc / SIM_DT;
            stats.simSeconds = static_cast<double>(stats.simTicks) * SIM_DT;
            stats.frameMs = frameDt * 1000.0;
            stats.fps = 1.0 / smoothDt;
            stats.displayHz = window.refreshRate();
            stats.vsync = vsync;

            int w, h;
            window.drawableSize(w, h);
            glViewport(0, 0, w, h);
            glClearColor(0.05f, 0.16f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            if (app::drawOverlay(stats, input, polled, vsync)) window.setVsync(vsync);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            window.swap();
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "fatal: %s\n", ex.what());
        return 1;
    }
    return 0;
}
