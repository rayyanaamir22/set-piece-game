// Port of src/input/keymap.ts. Keys are DOM KeyboardEvent.code strings so bindings
// are identical to (and could be shared with) the web build.
#pragma once
#include <SDL3/SDL_scancode.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace platform {

enum class Action {
    AimLeft, AimRight, AimUp, AimDown,
    ContactLeft, ContactRight, ContactUp, ContactDown, Recenter,
    Shoot, Driven, Low, Dummy,
    RunupWider, RunupNarrower, RunupLonger, RunupShorter,
    ToggleFoot, NewSpot, Retry, Camera, Replay, PlaceBall,
    Count
};

struct ActionDef {
    Action id;
    const char* key;    // id string used by the web build / prefs
    const char* label;
    const char* group;
    std::vector<std::string> defaults;
};

const std::vector<ActionDef>& actions();

using Keymap = std::array<std::vector<std::string>, static_cast<size_t>(Action::Count)>;

Keymap defaultKeymap();
bool holds(const Keymap& map, Action a, std::string_view code);

// SDL scancode -> KeyboardEvent.code ("KeyA", "Space", "ShiftLeft", ...); empty if unmapped.
std::string_view codeFor(SDL_Scancode sc);

}  // namespace platform
