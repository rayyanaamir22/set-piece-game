// Developer overlay (Dear ImGui): frame timing, sim loop and live input state.
#pragma once
#include "platform/input.h"

namespace app {

struct FrameStats {
    double frameMs = 0;        // last frame time
    double fps = 0;            // smoothed
    double displayHz = 0;
    double alpha = 0;          // render interpolation factor
    int stepsThisFrame = 0;
    unsigned long long simTicks = 0;
    double simSeconds = 0;
    bool vsync = true;
    const char* glRenderer = "";
    const char* glVersion = "";
};

// Returns true if the user changed vsync.
bool drawOverlay(const FrameStats& stats, const platform::Input& input, const platform::Polled& polled, bool& vsync);

}  // namespace app
