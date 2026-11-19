#include "app/overlay.h"

#include <imgui.h>

namespace app {

bool drawOverlay(const FrameStats& s, const platform::Input& in, const platform::Polled& p, bool& vsync) {
    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);
    if (ImGui::Begin("Set Piece | B1 platform shell", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%.1f fps  (%.2f ms)   display %.0f Hz", s.fps, s.frameMs, s.displayHz);
        ImGui::Text("sim: %llu ticks (%.2f s), %d this frame, alpha %.3f", s.simTicks, s.simSeconds, s.stepsThisFrame, s.alpha);
        ImGui::TextDisabled("%s | %s", s.glRenderer, s.glVersion);
        if (ImGui::Checkbox("vsync", &vsync)) changed = true;

        ImGui::SeparatorText("Input (the sim's InputState)");
        const auto& i = p.input;
        ImGui::Text("aim axes      %+.2f %+.2f", i.aimX, i.aimY);
        ImGui::Text("aim mouse d   %+.4f %+.4f m", i.aimDeltaX, i.aimDeltaY);
        ImGui::Text("contact       %+.2f %+.2f", i.contactX, i.contactY);
        ImGui::Text("run-up        angle %+.2f  dist %+.2f", i.runupAngle, i.runupDist);
        ImGui::Text("shoot %d  driven %d  low %d  dummy %d", i.shoot, i.driven, i.low, i.dummy);
        ImGui::Text("foot %d  newSpot %d  retry %d", i.toggleFoot, i.newSpot, i.retry);
        ImGui::Text("edges: camera %d  replay %d  place %d", p.cameraToggle, p.replayToggle, p.placeToggle);

        ImGui::SeparatorText("Devices");
        ImGui::Text("mouse %s  (click to capture, Esc to release)  L%d R%d", in.mouseLocked() ? "captured" : "free", in.buttonLeft(), in.buttonRight());
        std::string keys;
        for (const auto& k : in.keysDown()) keys += k + " ";
        ImGui::Text("keys: %s", keys.empty() ? "-" : keys.c_str());
        if (in.pad()) ImGui::Text("gamepad: %s%s", SDL_GetGamepadName(in.pad()), in.usingPad ? " (active)" : "");
        else ImGui::TextDisabled("gamepad: none");
    }
    ImGui::End();
    return changed;
}

}  // namespace app
