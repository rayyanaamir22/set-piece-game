// Port of prefs.ts + hud/storage.ts: client preferences and the saved game setup, as one JSON file in the user dir.
#pragma once
#include <map>
#include <optional>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "platform/keymap.h"
#include "render/look.h"
#include "sim/Sim.h"

namespace game {

enum class Quality { Low, Medium, High };
inline constexpr double REPLAY_SPEEDS[3] = {0.25, 0.5, 1};

// Everything that is about *your* setup rather than the rules of the free kick.
struct ClientPrefs {
    bool mouseAim = true;
    double mouseSensitivity = 1;
    double masterVolume = 0.7, crowdVolume = 0.7;
    bool muted = false;
    Quality quality = Quality::High;
    render::TimeOfDay timeOfDay = render::TimeOfDay::Day;
    double cameraDistance = 1;  // multiplier on the camera distance
    double replaySpeed = 0.5;
    bool autoReplay = false;
    platform::Keymap keymap = platform::defaultKeymap();
    sim::Mode mode = sim::Mode::Free;
    int takerIndex = 0, challengeIndex = 0;
    std::string difficulty = "normal";  // easy | normal | hard | pro | custom
    std::map<std::string, std::string> medals;  // challenge id -> bronze|silver|gold
    std::map<std::string, double> best;
    std::map<std::string, bool> collapsed;  // settings sections the player toggled
};

// The player's own game setup (not the temporary rules of a running challenge).
struct SavedGame {
    bool present = false;
    sim::SimSettings settings;
    std::optional<std::array<double, 3>> taker;  // accuracy, curve, power
    std::optional<char> foot;
    std::optional<std::array<double, 2>> runup;  // angle, dist
    bool showPath = false;
};

struct Prefs {
    ClientPrefs client;
    SavedGame game;
};

std::string prefsPath();
// Accepts anything (stored JSON) and returns valid prefs.
Prefs prefsFromJson(const nlohmann::json& root);
nlohmann::json prefsToJson(const Prefs& p);
Prefs loadPrefs(const std::string& path);
void savePrefs(const std::string& path, const Prefs& p);

const char* qualityName(Quality q);

}  // namespace game
