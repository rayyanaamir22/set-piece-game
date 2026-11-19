#include "platform/keymap.h"

#include <unordered_map>

namespace platform {

const std::vector<ActionDef>& actions() {
    static const std::vector<ActionDef> a = {
        {Action::AimLeft, "aimLeft", "Aim left", "Aim", {"KeyA", "ArrowLeft"}},
        {Action::AimRight, "aimRight", "Aim right", "Aim", {"KeyD", "ArrowRight"}},
        {Action::AimUp, "aimUp", "Aim up", "Aim", {"KeyW", "ArrowUp"}},
        {Action::AimDown, "aimDown", "Aim down", "Aim", {"KeyS", "ArrowDown"}},
        {Action::ContactLeft, "contactLeft", "Contact left", "Contact point", {"KeyJ"}},
        {Action::ContactRight, "contactRight", "Contact right", "Contact point", {"KeyL"}},
        {Action::ContactUp, "contactUp", "Contact up", "Contact point", {"KeyI"}},
        {Action::ContactDown, "contactDown", "Contact down", "Contact point", {"KeyK"}},
        {Action::Recenter, "recenter", "Re-centre contact", "Contact point", {"KeyO"}},
        {Action::Shoot, "shoot", "Shoot (hold, release)", "Shoot", {"Space"}},
        {Action::Driven, "driven", "Driven shot (hold)", "Shot type", {"ShiftLeft", "ShiftRight"}},
        {Action::Low, "low", "Low drive (hold)", "Shot type", {"ControlLeft", "KeyZ"}},
        {Action::Dummy, "dummy", "Dummy runner (hold at release)", "Shot type", {"KeyX"}},
        {Action::RunupWider, "runupWider", "Run-up wider", "Run-up", {"KeyE"}},
        {Action::RunupNarrower, "runupNarrower", "Run-up straighter", "Run-up", {"KeyQ"}},
        {Action::RunupLonger, "runupLonger", "Run-up longer", "Run-up", {"BracketRight"}},
        {Action::RunupShorter, "runupShorter", "Run-up shorter", "Run-up", {"BracketLeft"}},
        {Action::ToggleFoot, "toggleFoot", "Switch foot", "Free kick", {"KeyF"}},
        {Action::NewSpot, "newSpot", "New spot / next", "Free kick", {"KeyN"}},
        {Action::Retry, "retry", "Retry / restart", "Free kick", {"KeyR"}},
        {Action::PlaceBall, "placeBall", "Place ball (practice)", "Free kick", {"KeyP"}},
        {Action::Camera, "camera", "Near / far camera", "View", {"KeyC"}},
        {Action::Replay, "replay", "Instant replay", "View", {"KeyV"}},
    };
    return a;
}

Keymap defaultKeymap() {
    Keymap m;
    for (const auto& a : actions()) m[static_cast<size_t>(a.id)] = a.defaults;
    return m;
}

bool holds(const Keymap& map, Action a, std::string_view code) {
    for (const auto& c : map[static_cast<size_t>(a)])
        if (c == code) return true;
    return false;
}

std::string_view codeFor(SDL_Scancode sc) {
    static const std::unordered_map<int, std::string> table = [] {
        std::unordered_map<int, std::string> t;
        for (int i = 0; i < 26; ++i) t[SDL_SCANCODE_A + i] = std::string("Key") + char('A' + i);
        for (int i = 0; i < 9; ++i) t[SDL_SCANCODE_1 + i] = std::string("Digit") + char('1' + i);
        t[SDL_SCANCODE_0] = "Digit0";
        t[SDL_SCANCODE_SPACE] = "Space";
        t[SDL_SCANCODE_LSHIFT] = "ShiftLeft";
        t[SDL_SCANCODE_RSHIFT] = "ShiftRight";
        t[SDL_SCANCODE_LCTRL] = "ControlLeft";
        t[SDL_SCANCODE_RCTRL] = "ControlRight";
        t[SDL_SCANCODE_LALT] = "AltLeft";
        t[SDL_SCANCODE_RALT] = "AltRight";
        t[SDL_SCANCODE_LEFT] = "ArrowLeft";
        t[SDL_SCANCODE_RIGHT] = "ArrowRight";
        t[SDL_SCANCODE_UP] = "ArrowUp";
        t[SDL_SCANCODE_DOWN] = "ArrowDown";
        t[SDL_SCANCODE_LEFTBRACKET] = "BracketLeft";
        t[SDL_SCANCODE_RIGHTBRACKET] = "BracketRight";
        t[SDL_SCANCODE_RETURN] = "Enter";
        t[SDL_SCANCODE_TAB] = "Tab";
        t[SDL_SCANCODE_BACKSPACE] = "Backspace";
        t[SDL_SCANCODE_COMMA] = "Comma";
        t[SDL_SCANCODE_PERIOD] = "Period";
        t[SDL_SCANCODE_SLASH] = "Slash";
        t[SDL_SCANCODE_SEMICOLON] = "Semicolon";
        t[SDL_SCANCODE_APOSTROPHE] = "Quote";
        t[SDL_SCANCODE_MINUS] = "Minus";
        t[SDL_SCANCODE_EQUALS] = "Equal";
        t[SDL_SCANCODE_GRAVE] = "Backquote";
        t[SDL_SCANCODE_BACKSLASH] = "Backslash";
        return t;
    }();
    auto it = table.find(static_cast<int>(sc));
    return it == table.end() ? std::string_view{} : std::string_view{it->second};
}

}  // namespace platform
