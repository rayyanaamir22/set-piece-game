// Ported from tests/input.test.ts: mouse helpers, keymap and client prefs. (Audio cues arrive with B9.)
#include <doctest/doctest.h>

#include <map>
#include <nlohmann/json.hpp>

#include "game/prefs.h"
#include "platform/keymap.h"
#include "platform/mouse.h"

using namespace platform;
using nlohmann::json;
using game::ClientPrefs;
using game::Prefs;

TEST_SUITE("Mouse helpers") {
    TEST_CASE("moving right/up moves the reticle right/up (screen y grows downward)") {
        const Vec2 d = aimDeltaFromMouse(100, -50, 1);
        CHECK(d.x == doctest::Approx(100 * AIM_M_PER_PX));
        CHECK(d.y == doctest::Approx(50 * AIM_M_PER_PX));
        CHECK(aimDeltaFromMouse(0, 40, 1).y < 0);
    }
    TEST_CASE("sensitivity scales linearly and is clamped") {
        CHECK(aimDeltaFromMouse(100, 0, 2).x == doctest::Approx(2 * aimDeltaFromMouse(100, 0, 1).x));
        CHECK(clampSens(99) == SENS_MAX);
        CHECK(clampSens(0) == SENS_MIN);
        CHECK(clampSens(std::nan("")) == 1);
    }
    TEST_CASE("the contact cursor never leaves the unit disc, and mouse up moves it up") {
        Vec2 c{0, 0};
        c = moveContactCursor(c, 0, -100, 1);
        CHECK(c.y > 0);
        for (int i = 0; i < 50; i++) c = moveContactCursor(c, 300, -300, 1);
        CHECK(sim::jhypot(c.x, c.y) <= 1 + 1e-9);
    }
}

TEST_SUITE("Keymap") {
    TEST_CASE("ships a unique default key for every action (no accidental clashes)") {
        std::map<std::string, std::string> used;
        for (const auto& a : actions())
            for (const auto& c : a.defaults) {
                INFO(c << " used by " << used[c] << " and " << a.key);
                CHECK(used.count(c) == 0);
                used[c] = a.key;
                CHECK_FALSE(isReserved(c));
            }
    }

    TEST_CASE("rebinding replaces the primary key and keeps the secondary one") {
        const Keymap m = rebind(defaultKeymap(), Action::AimLeft, "KeyH");
        CHECK(m[static_cast<size_t>(Action::AimLeft)] == std::vector<std::string>{"KeyH", "ArrowLeft"});
        CHECK(holds(m, Action::AimLeft, "KeyH"));
        CHECK_FALSE(holds(m, Action::AimLeft, "KeyA"));
    }

    TEST_CASE("a key taken from another action is removed from it") {
        const Keymap m = rebind(defaultKeymap(), Action::Camera, "KeyF");  // F was "switch foot"
        CHECK(m[static_cast<size_t>(Action::Camera)][0] == "KeyF");
        const auto c = conflicts(m, "KeyF");
        REQUIRE(c.size() == 1);
        CHECK(c[0] == Action::Camera);
        CHECK(m[static_cast<size_t>(Action::ToggleFoot)] == std::vector<std::string>{"KeyC"});  // swapped
    }

    TEST_CASE("every action is still bound after any single rebind") {
        for (const auto& a : actions()) {
            const Keymap m = rebind(defaultKeymap(), a.id, "KeyY");
            for (const auto& b : actions()) CHECK(m[static_cast<size_t>(b.id)].size() > 0);
            const auto c = conflicts(m, "KeyY");
            REQUIRE(c.size() == 1);
            CHECK(c[0] == a.id);
        }
    }

    TEST_CASE("Escape cannot be bound") {
        const Keymap m = defaultKeymap();
        CHECK(rebind(m, Action::Shoot, "Escape") == m);
        Prefs p = game::prefsFromJson(json{{"client", {{"keymap", {{"shoot", json::array({"Escape"})}}}}}});
        CHECK(p.client.keymap[static_cast<size_t>(Action::Shoot)] == std::vector<std::string>{"Space"});
    }

    TEST_CASE("sanitize repairs garbage") {
        const Keymap def = defaultKeymap();
        CHECK(game::prefsFromJson(nullptr).client.keymap == def);
        CHECK(game::prefsFromJson(json{{"client", {{"keymap", {{"shoot", "x"}, {"aimLeft", json::array()}, {"driven", json::array({1, 2})}}}}}}).client.keymap == def);
        const Prefs ok = game::prefsFromJson(json{{"client", {{"keymap", {{"shoot", json::array({"Enter"})}, {"bogus", json::array({"KeyQ"})}}}}}});
        CHECK(ok.client.keymap[static_cast<size_t>(Action::Shoot)] == std::vector<std::string>{"Enter"});
    }

    TEST_CASE("labels keys for humans") {
        CHECK(keyLabel("KeyA") == "A");
        CHECK(keyLabel("ArrowLeft") == "Left");
        CHECK(keyLabel("BracketRight") == "]");
        CHECK(keyLabel("Digit3") == "3");
        CHECK(keyLabel("ShiftRight") == "Shift");
        CHECK(keyLabel("Space") == "Space");
    }

    TEST_CASE("SDL scancodes map to the same DOM key codes the web build binds") {
        CHECK(codeFor(SDL_SCANCODE_A) == "KeyA");
        CHECK(codeFor(SDL_SCANCODE_SPACE) == "Space");
        CHECK(codeFor(SDL_SCANCODE_LSHIFT) == "ShiftLeft");
        CHECK(codeFor(SDL_SCANCODE_LEFTBRACKET) == "BracketLeft");
        CHECK(codeFor(SDL_SCANCODE_LEFT) == "ArrowLeft");
        // Every default binding is reachable from a physical key.
        for (const auto& a : actions())
            for (const auto& c : a.defaults) {
                bool found = false;
                for (int sc = 0; sc < SDL_SCANCODE_COUNT && !found; sc++) found = codeFor(static_cast<SDL_Scancode>(sc)) == c;
                INFO(c);
                CHECK(found);
            }
    }
}

namespace {
json dump(const Prefs& p) { return game::prefsToJson(p); }
}

TEST_SUITE("Client prefs") {
    TEST_CASE("defaults are valid and stable through sanitize") {
        const Prefs d;
        CHECK(dump(game::prefsFromJson(dump(d))) == dump(d));
        CHECK(dump(game::prefsFromJson(nullptr)) == dump(d));
        CHECK(dump(game::prefsFromJson("nonsense")) == dump(d));
    }
    TEST_CASE("clamps numbers and rejects bad enums") {
        const Prefs p = game::prefsFromJson(json{{"client", {{"mouseSensitivity", 99}, {"masterVolume", -3}, {"cameraDistance", 0}, {"quality", "ultra"},
                                                              {"timeOfDay", "night"}, {"replaySpeed", 7}, {"mode", "x"}, {"difficulty", "pro"}}}});
        const ClientPrefs& c = p.client;
        CHECK(c.mouseSensitivity == 3);
        CHECK(c.masterVolume == 0);
        CHECK(c.cameraDistance == 0.6);
        CHECK(c.quality == game::Quality::High);
        CHECK(c.timeOfDay == render::TimeOfDay::Night);
        CHECK(c.replaySpeed == 0.5);
        CHECK(c.mode == sim::Mode::Free);
        CHECK(c.difficulty == "pro");
    }
    TEST_CASE("keeps valid medals/bests and drops junk") {
        const Prefs p = game::prefsFromJson(json{{"client", {{"medals", {{"a", "gold"}, {"b", "platinum"}, {"c", nullptr}}}, {"best", {{"a", 12}, {"b", "x"}}}, {"collapsed", {{"s", true}, {"t", 1}}}}}});
        CHECK(p.client.medals == std::map<std::string, std::string>{{"a", "gold"}});
        CHECK(p.client.best == std::map<std::string, double>{{"a", 12}});
        CHECK(p.client.collapsed == std::map<std::string, bool>{{"s", true}});
    }
    TEST_CASE("round-trips through JSON, including the saved game setup") {
        Prefs p;
        p.client.mouseSensitivity = 1.7;
        p.client.quality = game::Quality::Low;
        p.client.timeOfDay = render::TimeOfDay::Dusk;
        p.client.mode = sim::Mode::Challenge;
        p.client.challengeIndex = 2;
        p.client.difficulty = "custom";
        p.client.keymap = rebind(p.client.keymap, Action::Camera, "KeyG");
        p.game.present = true;
        p.game.settings.windEnabled = true;
        p.game.settings.keeperType = sim::KeeperType::Quick;
        p.game.settings.lyingMan = sim::Frequency::Always;
        p.game.taker = std::array<double, 3>{0.5, 0.6, 0.9};
        p.game.foot = 'L';
        p.game.runup = std::array<double, 2>{50, 4.5};
        p.game.showPath = true;
        const Prefs q = game::prefsFromJson(dump(p));
        CHECK(dump(q) == dump(p));
        CHECK(q.game.settings.keeperType == sim::KeeperType::Quick);
        CHECK(q.game.foot == 'L');
    }
}
