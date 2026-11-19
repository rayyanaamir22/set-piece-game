// Port of src/input/Input.ts + mouse.ts: keyboard + mouse + gamepad -> sim::InputState.
#pragma once
#include <SDL3/SDL.h>

#include <set>
#include <string>

#include "platform/keymap.h"
#include "sim/input_state.h"

namespace platform {

constexpr double AIM_M_PER_PX = 0.012;
constexpr double CONTACT_PER_PX = 0.006;
constexpr double SENS_MIN = 0.2, SENS_MAX = 3;
double clampSens(double s);

struct Polled {
    sim::InputState input;
    // One-shot edges handled by the app/view rather than the sim.
    bool cameraToggle = false, replayToggle = false, placeToggle = false;
};

class Input {
public:
    Keymap keymap = defaultKeymap();
    bool mouseAim = true;
    double sensitivity = 1;
    bool usingPad = false;

    // Feed every SDL event (after ImGui has seen it). Returns nothing; state is internal.
    void handleEvent(const SDL_Event& e, SDL_Window* window);
    // Frame poll; dt in seconds.
    Polled poll(double dt);

    bool mouseLocked() const { return locked_; }
    void setMouseLocked(SDL_Window* w, bool on);
    // Disable while a UI panel has focus so typing doesn't steer the game.
    void setEnabled(SDL_Window* w, bool on);
    bool enabled() const { return enabled_; }

    // Raw state for the debug overlay.
    const std::set<std::string>& keysDown() const { return down_; }
    SDL_Gamepad* pad() const { return pad_; }
    bool buttonLeft() const { return left_; }
    bool buttonRight() const { return right_; }

private:
    bool held(Action a) const;
    void mouseMove(double dx, double dy, bool contact);
    void openFirstPad();

    std::set<std::string> down_;
    std::set<std::string> latched_;  // pressed since last poll: sub-frame taps still count
    double cx_ = 0, cy_ = 0;         // contact cursor
    double accDx_ = 0, accDy_ = 0;
    bool left_ = false, right_ = false, dragging_ = false;
    bool enabled_ = true, locked_ = false;
    bool edgeCamera_ = false, edgeReplay_ = false, edgePlace_ = false;
    bool prevPadCamera_ = false, prevPadReplay_ = false;
    SDL_Gamepad* pad_ = nullptr;
};

}  // namespace platform
