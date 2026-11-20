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

// Keys that can never be bound: Esc always closes menus / frees the mouse.
bool isReserved(std::string_view code);
// Which actions currently use `code` (optionally ignoring one).
std::vector<Action> conflicts(const Keymap& map, std::string_view code, int except = -1);
// Make `code` the primary key of `id`. Any other action that used the key loses it; if that leaves it with
// no key at all, it takes over the key `id` just gave up (a swap).
Keymap rebind(const Keymap& map, Action id, const std::string& code);
// Human label for a KeyboardEvent.code ("KeyA" -> "A", "ArrowLeft" -> "Left", ...).
std::string keyLabel(const std::string& code);
const ActionDef& actionDef(Action a);

// SDL scancode -> KeyboardEvent.code ("KeyA", "Space", "ShiftLeft", ...); empty if unmapped.
std::string_view codeFor(SDL_Scancode sc);

}  // namespace platform
