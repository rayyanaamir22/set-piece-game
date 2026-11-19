#include "platform/input.h"

#include <algorithm>
#include <cmath>

namespace platform {

namespace {
constexpr double DEAD = 0.18;
double dz(double v) { return std::fabs(v) < DEAD ? 0.0 : v; }
}  // namespace

double clampSens(double s) { return std::isfinite(s) ? std::min(SENS_MAX, std::max(SENS_MIN, s)) : 1.0; }

void Input::setMouseLocked(SDL_Window* w, bool on) {
    if (on == locked_) return;
    locked_ = on;
    SDL_SetWindowRelativeMouseMode(w, on);
    if (!on) {
        left_ = right_ = dragging_ = false;
    }
}

void Input::setEnabled(SDL_Window* w, bool on) {
    enabled_ = on;
    if (!on) {
        down_.clear();
        latched_.clear();
        edgeCamera_ = edgeReplay_ = edgePlace_ = false;
        accDx_ = accDy_ = 0;
        left_ = right_ = dragging_ = false;
        setMouseLocked(w, false);
    }
}

void Input::openFirstPad() {
    if (pad_) return;
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    if (ids && n > 0) pad_ = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
}

void Input::mouseMove(double dx, double dy, bool contact) {
    if (contact) {
        const double k = CONTACT_PER_PX * clampSens(sensitivity);
        double x = cx_ + dx * k, y = cy_ - dy * k;
        const double m = std::hypot(x, y);
        if (m > 1) { x /= m; y /= m; }
        cx_ = x; cy_ = y;
    } else {
        accDx_ += dx;
        accDy_ += dy;
    }
}

void Input::handleEvent(const SDL_Event& e, SDL_Window* window) {
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN: {
        if (!enabled_ || e.key.repeat) break;
        if (e.key.scancode == SDL_SCANCODE_ESCAPE) { setMouseLocked(window, false); break; }  // Esc frees the mouse, never bindable
        const std::string_view code = codeFor(e.key.scancode);
        if (code.empty()) break;
        const std::string c(code);
        down_.insert(c);
        latched_.insert(c);
        if (holds(keymap, Action::Camera, c)) edgeCamera_ = true;
        if (holds(keymap, Action::Replay, c)) edgeReplay_ = true;
        if (holds(keymap, Action::PlaceBall, c)) edgePlace_ = true;
        if (holds(keymap, Action::Recenter, c)) cx_ = cy_ = 0;
        break;
    }
    case SDL_EVENT_KEY_UP: {
        const std::string_view code = codeFor(e.key.scancode);
        if (!code.empty()) down_.erase(std::string(code));
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        down_.clear();
        left_ = right_ = dragging_ = false;
        setMouseLocked(window, false);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!enabled_) break;
        if (e.button.button == SDL_BUTTON_LEFT) {
            left_ = true;
            if (mouseAim && !locked_) setMouseLocked(window, true);  // first click captures the pointer
        }
        if (e.button.button == SDL_BUTTON_RIGHT) right_ = true;
        dragging_ = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) left_ = false;
        if (e.button.button == SDL_BUTTON_RIGHT) right_ = false;
        if (!left_ && !right_) dragging_ = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (!enabled_ || !mouseAim || !locked_) break;
        mouseMove(e.motion.xrel, e.motion.yrel, right_);
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        openFirstPad();
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (pad_ && SDL_GetGamepadID(pad_) == e.gdevice.which) {
            SDL_CloseGamepad(pad_);
            pad_ = nullptr;
            usingPad = false;
            openFirstPad();
        }
        break;
    default: break;
    }
}

bool Input::held(Action a) const {
    for (const auto& c : keymap[static_cast<size_t>(a)])
        if (down_.count(c) || latched_.count(c)) return true;
    return false;
}

Polled Input::poll(double dt) {
    Polled out;
    sim::InputState& i = out.input;
    auto h = [&](Action a) { return held(a) ? 1.0 : 0.0; };

    i.aimX = h(Action::AimRight) - h(Action::AimLeft);
    i.aimY = h(Action::AimUp) - h(Action::AimDown);
    i.runupAngle = h(Action::RunupWider) - h(Action::RunupNarrower);
    i.runupDist = h(Action::RunupLonger) - h(Action::RunupShorter);

    constexpr double rate = 1.8;
    cx_ += (h(Action::ContactRight) - h(Action::ContactLeft)) * rate * dt;
    cy_ += (h(Action::ContactUp) - h(Action::ContactDown)) * rate * dt;
    const double m = std::hypot(cx_, cy_);
    if (m > 1) { cx_ /= m; cy_ /= m; }
    i.contactX = cx_;
    i.contactY = cy_;

    // Mouse reticle movement: applied once per frame by the app (not per sim step).
    const double k = AIM_M_PER_PX * clampSens(sensitivity);
    i.aimDeltaX = accDx_ * k;
    i.aimDeltaY = -accDy_ * k;
    accDx_ = accDy_ = 0;

    i.shoot = held(Action::Shoot) || (locked_ && left_);
    i.driven = held(Action::Driven);
    i.low = held(Action::Low);
    i.dummy = held(Action::Dummy);
    i.toggleFoot = held(Action::ToggleFoot);
    i.retry = held(Action::Retry);
    i.newSpot = held(Action::NewSpot);

    bool cam = edgeCamera_, rep = edgeReplay_;
    out.placeToggle = edgePlace_;
    edgeCamera_ = edgeReplay_ = edgePlace_ = false;

    if (!pad_) openFirstPad();
    if (pad_) {
        // Web "standard" mapping indices -> SDL gamepad buttons.
        auto b = [&](SDL_GamepadButton n) { return SDL_GetGamepadButton(pad_, n); };
        auto ax = [&](SDL_GamepadAxis n) { return dz(SDL_GetGamepadAxis(pad_, n) / 32767.0); };
        auto trig = [&](SDL_GamepadAxis n) { return std::max(0.0, SDL_GetGamepadAxis(pad_, n) / 32767.0); };
        const double lx = ax(SDL_GAMEPAD_AXIS_LEFTX), ly = ax(SDL_GAMEPAD_AXIS_LEFTY);
        const double rx = ax(SDL_GAMEPAD_AXIS_RIGHTX), ry = ax(SDL_GAMEPAD_AXIS_RIGHTY);
        bool anyButton = false;
        for (int n = 0; n < SDL_GAMEPAD_BUTTON_COUNT; ++n) anyButton = anyButton || SDL_GetGamepadButton(pad_, static_cast<SDL_GamepadButton>(n));
        if (lx != 0 || ly != 0 || rx != 0 || ry != 0 || anyButton) usingPad = true;
        if (lx != 0 || ly != 0) { i.aimX = lx; i.aimY = -ly; }
        if (usingPad) {
            i.contactX = rx;
            i.contactY = -ry;
            cx_ = rx;
            cy_ = -ry;
        }
        const double t = trig(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) - trig(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        if (std::fabs(t) > 0.1) i.runupAngle = t;
        const bool up = b(SDL_GAMEPAD_BUTTON_DPAD_UP), down = b(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        if (up || down) i.runupDist = (up ? 1 : 0) - (down ? 1 : 0);
        i.shoot = i.shoot || b(SDL_GAMEPAD_BUTTON_EAST);              // web 1 (B / circle)
        i.driven = i.driven || b(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);  // web 5
        i.low = i.low || b(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);         // web 4
        i.dummy = i.dummy || b(SDL_GAMEPAD_BUTTON_LEFT_STICK);        // web 10
        i.toggleFoot = i.toggleFoot || b(SDL_GAMEPAD_BUTTON_NORTH);   // web 3 (Y / triangle)
        i.newSpot = i.newSpot || b(SDL_GAMEPAD_BUTTON_WEST);          // web 2 (X / square)
        i.retry = i.retry || b(SDL_GAMEPAD_BUTTON_START);             // web 9
        const bool camBtn = b(SDL_GAMEPAD_BUTTON_BACK);               // web 8
        if (camBtn && !prevPadCamera_) cam = true;
        prevPadCamera_ = camBtn;
        const bool repBtn = b(SDL_GAMEPAD_BUTTON_RIGHT_STICK);        // web 11
        if (repBtn && !prevPadReplay_) rep = true;
        prevPadReplay_ = repBtn;
    }

    latched_.clear();
    out.cameraToggle = cam;
    out.replayToggle = rep;
    return out;
}

}  // namespace platform
