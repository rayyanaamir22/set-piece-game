#include "game/prefs.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "sim/config_io.h"

namespace game {

using json = nlohmann::json;

namespace {
double num(const json& j, const char* k, double lo, double hi, double d) {
    if (!j.contains(k) || !j[k].is_number()) return d;
    const double v = j[k].get<double>();
    return std::isfinite(v) ? std::min(hi, std::max(lo, v)) : d;
}
bool boolean(const json& j, const char* k, bool d) { return j.contains(k) && j[k].is_boolean() ? j[k].get<bool>() : d; }

const char* timeName(render::TimeOfDay t) { return t == render::TimeOfDay::Day ? "day" : t == render::TimeOfDay::Dusk ? "dusk" : "night"; }
}  // namespace

const char* qualityName(Quality q) { return q == Quality::Low ? "low" : q == Quality::Medium ? "medium" : "high"; }

std::string prefsPath() {
    char* dir = SDL_GetPrefPath("SetPiece", "SetPiece");
    std::string p = dir ? std::string(dir) + "prefs.json" : "prefs.json";
    SDL_free(dir);
    return p;
}

Prefs loadPrefs(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    return prefsFromJson(json::parse(f, nullptr, /*allow_exceptions=*/false));
}

Prefs prefsFromJson(const json& root) {
    Prefs out;
    if (root.is_discarded() || !root.is_object()) return out;

    if (root.contains("client") && root["client"].is_object()) {
        const json& c = root["client"];
        ClientPrefs& d = out.client;
        d.mouseAim = boolean(c, "mouseAim", d.mouseAim);
        d.mouseSensitivity = num(c, "mouseSensitivity", 0.2, 3, d.mouseSensitivity);
        d.masterVolume = num(c, "masterVolume", 0, 1, d.masterVolume);
        d.crowdVolume = num(c, "crowdVolume", 0, 1, d.crowdVolume);
        d.muted = boolean(c, "muted", d.muted);
        if (c.contains("quality") && c["quality"].is_string()) {
            const std::string q = c["quality"];
            d.quality = q == "low" ? Quality::Low : q == "medium" ? Quality::Medium : Quality::High;
        }
        if (c.contains("timeOfDay") && c["timeOfDay"].is_string()) {
            const std::string t = c["timeOfDay"];
            d.timeOfDay = t == "dusk" ? render::TimeOfDay::Dusk : t == "night" ? render::TimeOfDay::Night : render::TimeOfDay::Day;
        }
        d.cameraDistance = num(c, "cameraDistance", 0.6, 1.8, d.cameraDistance);
        if (c.contains("replaySpeed") && c["replaySpeed"].is_number()) {
            const double v = c["replaySpeed"];
            if (std::find(std::begin(REPLAY_SPEEDS), std::end(REPLAY_SPEEDS), v) != std::end(REPLAY_SPEEDS)) d.replaySpeed = v;
        }
        d.autoReplay = boolean(c, "autoReplay", d.autoReplay);
        if (c.contains("keymap") && c["keymap"].is_object()) {
            for (const auto& a : platform::actions()) {
                if (!c["keymap"].contains(a.key) || !c["keymap"][a.key].is_array()) continue;
                std::vector<std::string> keys;
                bool ok = !c["keymap"][a.key].empty();
                for (const auto& k : c["keymap"][a.key]) {
                    if (!k.is_string() || k.get<std::string>().empty() || platform::isReserved(k.get<std::string>())) ok = false;
                    else if (std::find(keys.begin(), keys.end(), k.get<std::string>()) == keys.end()) keys.push_back(k);
                }
                if (ok) d.keymap[static_cast<size_t>(a.id)] = keys;
            }
        }
        if (c.contains("mode") && c["mode"].is_string()) {
            const std::string m = c["mode"];
            d.mode = m == "practice" ? sim::Mode::Practice : m == "challenge" ? sim::Mode::Challenge : sim::Mode::Free;
        }
        d.takerIndex = static_cast<int>(num(c, "takerIndex", 0, 99, 0));
        d.challengeIndex = static_cast<int>(num(c, "challengeIndex", 0, 99, 0));
        if (c.contains("difficulty") && c["difficulty"].is_string()) {
            const std::string s = c["difficulty"];
            if (s == "easy" || s == "normal" || s == "hard" || s == "pro" || s == "custom") d.difficulty = s;
        }
        if (c.contains("medals") && c["medals"].is_object())
            for (auto it = c["medals"].begin(); it != c["medals"].end(); ++it)
                if (it.value().is_string()) {
                    const std::string m = it.value();
                    if (m == "bronze" || m == "silver" || m == "gold") d.medals[it.key()] = m;
                }
        if (c.contains("best") && c["best"].is_object())
            for (auto it = c["best"].begin(); it != c["best"].end(); ++it)
                if (it.value().is_number()) d.best[it.key()] = it.value();
        if (c.contains("collapsed") && c["collapsed"].is_object())
            for (auto it = c["collapsed"].begin(); it != c["collapsed"].end(); ++it)
                if (it.value().is_boolean()) d.collapsed[it.key()] = it.value();
    }

    if (root.contains("game") && root["game"].is_object()) {
        const json& g = root["game"];
        out.game.present = true;
        out.game.settings = sim::sanitizeSettings(sim::parseSettings(g.value("settings", json::object())));
        if (g.contains("taker") && g["taker"].is_object())
            out.game.taker = std::array<double, 3>{num(g["taker"], "accuracy", 0, 1, 0.8), num(g["taker"], "curve", 0, 1.5, 0.9), num(g["taker"], "power", 0.7, 1.2, 1)};
        if (g.contains("foot") && g["foot"].is_string() && (g["foot"] == "R" || g["foot"] == "L")) out.game.foot = g["foot"].get<std::string>()[0];
        if (g.contains("runup") && g["runup"].is_object())
            out.game.runup = std::array<double, 2>{num(g["runup"], "angle", 0, 1e3, 35), num(g["runup"], "dist", 0, 1e3, 3.5)};
        out.game.showPath = boolean(g, "showPath", false);
    }
    return out;
}

json prefsToJson(const Prefs& p) {
    json c;
    const ClientPrefs& d = p.client;
    c["mouseAim"] = d.mouseAim;
    c["mouseSensitivity"] = d.mouseSensitivity;
    c["masterVolume"] = d.masterVolume;
    c["crowdVolume"] = d.crowdVolume;
    c["muted"] = d.muted;
    c["quality"] = qualityName(d.quality);
    c["timeOfDay"] = timeName(d.timeOfDay);
    c["cameraDistance"] = d.cameraDistance;
    c["replaySpeed"] = d.replaySpeed;
    c["autoReplay"] = d.autoReplay;
    json km = json::object();
    for (const auto& a : platform::actions()) km[a.key] = d.keymap[static_cast<size_t>(a.id)];
    c["keymap"] = km;
    c["mode"] = sim::str(d.mode);
    c["takerIndex"] = d.takerIndex;
    c["challengeIndex"] = d.challengeIndex;
    c["difficulty"] = d.difficulty;
    c["medals"] = d.medals;
    c["best"] = d.best;
    c["collapsed"] = d.collapsed;

    json root;
    root["client"] = c;
    if (p.game.present) {
        const sim::SimSettings& s = p.game.settings;
        json g;
        g["settings"] = {{"wallEnabled", s.wallEnabled}, {"wallHeight", s.wallHeight}, {"wallCount", s.wallCount}, {"wallJumps", s.wallJumps},
                         {"lyingMan", sim::str(s.lyingMan)}, {"encroachment", sim::str(s.encroachment)}, {"rebounds", s.rebounds},
                         {"keeperType", sim::str(s.keeperType)}, {"keeperPresent", s.keeperPresent}, {"keeperSkill", s.keeperSkill},
                         {"windEnabled", s.windEnabled}, {"windSpeed", s.windSpeed}, {"windDirection", s.windDirection}};
        if (p.game.taker) g["taker"] = {{"accuracy", (*p.game.taker)[0]}, {"curve", (*p.game.taker)[1]}, {"power", (*p.game.taker)[2]}};
        if (p.game.foot) g["foot"] = std::string(1, *p.game.foot);
        if (p.game.runup) g["runup"] = {{"angle", (*p.game.runup)[0]}, {"dist", (*p.game.runup)[1]}};
        g["showPath"] = p.game.showPath;
        root["game"] = g;
    }
    return root;
}

void savePrefs(const std::string& path, const Prefs& p) {
    const json root = prefsToJson(p);
    // Write to a temp file and rename so a crash never leaves half a file.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp);
        if (!f) return;
        f << root.dump(1) << "\n";
    }
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());
}

}  // namespace game
