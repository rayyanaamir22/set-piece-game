// The game application: window, sim loop (fixed 240 Hz + interpolation), modes, preferences, HUD and sheets.
#pragma once
#include <array>
#include <memory>
#include <optional>
#include <string>

#include "game/prefs.h"
#include "game/ui.h"
#include "platform/input.h"
#include "platform/window.h"
#include "render/game_view.h"
#include "sim/Sim.h"

namespace game {

struct Options {
    std::optional<int32_t> seed;
    // Developer/CI helpers: render `frames` frames, optionally take a scripted shot, save a PNG and exit.
    std::string shotPath;
    int frames = 0;
    bool demoShoot = false;
    std::string openSheet;  // "settings" | "controls": start with that sheet open (screenshots)
    std::optional<std::pair<double, double>> demoAim;  // --aim X Y: set the reticle at start (screenshots)
    std::optional<std::pair<double, double>> demoContact;  // --contact X Y: contact point while shooting (screenshots)
    std::string debugCam;  // "taker" | "keeper"
    int bench = 0;  // run N frames with vsync off and print frame-time statistics
    std::string prefsOverride;  // use this prefs file instead of the user's
};

class App {
public:
    explicit App(const Options& opts);
    ~App();
    int run();

private:
    void frame(double frameDt);
    void persist();
    void applyClientPrefs();
    void setPlacing(bool on);
    void recordChallengeResult();
    void selectTakerIdx(int i);
    void resetAll();
    void screenshot(const std::string& path);

    Options opts_;
    platform::Window window_;
    platform::Input input_;
    std::shared_ptr<sim::SimConfig> cfg_;
    sim::SimState sim_;
    Prefs prefs_;
    std::string prefsPath_;
    std::unique_ptr<render::GameView> view_;
    Hud hud_;
    Sheets sheets_;
    SheetCtx ctx_;

    std::vector<std::array<double, 3>> takerStats_;  // per-taker accuracy/curve/power (edited in settings)
    std::vector<std::array<double, 3>> takerDefaults_;
    bool paused_ = false;
    bool placing_ = false;
    double acc_ = 0, pendDX_ = 0, pendDY_ = 0;
    bool recorded_ = false;
    int frameNo_ = 0;
    int demoState_ = 0;
    uint64_t last_ = 0;
    double smoothDt_ = 1.0 / 60;
    bool running_ = true;
    bool showStats_ = false;
    std::vector<double> frameTimes_;
    double gpuSum_ = 0;
};

}  // namespace game
