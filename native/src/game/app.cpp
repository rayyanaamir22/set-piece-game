#include "game/app.h"

#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstdio>
#include <random>

#include "sim/config_io.h"

namespace game {

namespace {
constexpr double SIM_DT = 1.0 / 240.0;  // same fixed step as the web build
constexpr double MAX_FRAME = 0.1;
const char* ASSET_DIR = SETPIECE_ASSET_DIR;
const char* DATA_DIR = SETPIECE_DATA_DIR;

int medalRank(const std::string& m) { return m == "gold" ? 3 : m == "silver" ? 2 : m == "bronze" ? 1 : 0; }
}  // namespace

App::App(const Options& opts)
    : opts_(opts),
      window_("Set Piece", 1280, 720),
      cfg_(sim::loadConfig(DATA_DIR)),
      sim_(sim::createSim(cfg_, opts.seed ? *opts.seed : static_cast<int32_t>(std::random_device{}() & 0x3fffffff))) {
    // ---- ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    applyStyle();
    loadFonts(ASSET_DIR);
    ImGui_ImplSDL3_InitForOpenGL(window_.handle(), window_.context());
    ImGui_ImplOpenGL3_Init("#version 410 core");

    view_ = std::make_unique<render::GameView>(cfg_->physics.radius, SETPIECE_SHADER_DIR);

    // ---- preferences
    prefsPath_ = opts.prefsOverride.empty() ? prefsPath() : opts.prefsOverride;
    prefs_ = loadPrefs(prefsPath_);
    for (const auto& t : cfg_->takers) takerDefaults_.push_back({t.accuracy, t.curve, t.power});
    takerStats_ = takerDefaults_;
    if (prefs_.game.present) {
        sim_.settings = prefs_.game.settings;
        if (prefs_.game.runup) {
            sim_.runup.angle = std::min(cfg_->shot.runupAngleMax, std::max(0.0, (*prefs_.game.runup)[0]));
            sim_.runup.dist = std::min(cfg_->shot.runupDistMax, std::max(cfg_->shot.runupDistMin, (*prefs_.game.runup)[1]));
        }
        sim_.showPath = prefs_.game.showPath;
    }
    const int ti = prefs_.client.takerIndex % static_cast<int>(cfg_->takers.size());
    if (prefs_.game.present && prefs_.game.taker) takerStats_[ti] = *prefs_.game.taker;
    selectTakerIdx(ti);
    if (prefs_.game.present && prefs_.game.foot) sim_.foot = *prefs_.game.foot;
    sim_.difficulty = prefs_.client.difficulty;
    if (prefs_.client.difficulty != "custom") {
        for (size_t i = 0; i < sim::DIFFICULTY_NAMES.size(); i++)
            if (prefs_.client.difficulty == sim::DIFFICULTY_NAMES[i]) sim_.accuracyBonus = cfg_->presets[i].accuracyBonus;
    }
    sim::applySettings(sim_);
    if (prefs_.client.mode == sim::Mode::Challenge) sim::startChallenge(sim_, prefs_.client.challengeIndex);
    else sim::setMode(sim_, prefs_.client.mode);

    // ---- sheets wiring (the equivalent of ClientCtx in main.ts)
    ctx_.sim = &sim_;
    ctx_.prefs = &prefs_.client;
    ctx_.apply = [this] { applyClientPrefs(); persist(); };
    ctx_.onSimChange = [this] { persist(); };
    ctx_.setMode = [this](sim::Mode m) { sim::setMode(sim_, m, prefs_.client.challengeIndex); setPlacing(false); persist(); };
    ctx_.startChallenge = [this](int i) { sim::startChallenge(sim_, i); setPlacing(false); persist(); };
    ctx_.selectTaker = [this](int i) { selectTakerIdx(i); persist(); };
    ctx_.applyDifficulty = [this](sim::DifficultyName n) { sim::applyPreset(sim_, n); persist(); };
    ctx_.togglePlacing = [this] { setPlacing(!placing_); };
    ctx_.isPlacing = [this] { return placing_; };
    ctx_.resetAll = [this] { resetAll(); };
    ctx_.setKeymap = [this](const platform::Keymap& k) { prefs_.client.keymap = k; applyClientPrefs(); persist(); };

    input_.onGroundClick = [this](double nx, double ny) {
        const auto p = view_->pickGround(nx, ny);
        if (p && sim_.phase == sim::Phase::Aim && sim_.mode == sim::Mode::Practice) {
            sim::setSpot(sim_, std::clamp<double>(p->x, -34, 34), std::clamp<double>(p->y, 2.5, 48));
            persist();
        }
        setPlacing(false);
    };

    applyClientPrefs();
    if (opts.bench > 0) window_.setVsync(false);
    view_->setDebugCamera(opts.debugCam);
    if (opts.demoAim) sim_.aim = {opts.demoAim->first, opts.demoAim->second};
    if (opts.openSheet == "settings") sheets_.open = Sheet::Settings;
    if (opts.openSheet == "controls") sheets_.open = Sheet::Controls;
    last_ = SDL_GetPerformanceCounter();
}

App::~App() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void App::applyClientPrefs() {
    const ClientPrefs& c = prefs_.client;
    view_->setQuality(c.quality);
    view_->setTimeOfDay(c.timeOfDay);
    view_->rig.distanceMul = c.cameraDistance;
    input_.keymap = c.keymap;
    input_.mouseAim = c.mouseAim;
    input_.sensitivity = platform::clampSens(c.mouseSensitivity);
    if (!c.mouseAim) input_.setMouseLocked(window_.handle(), false);
}

void App::persist() {
    // While a challenge is running its rules are temporary: remember the player's own settings instead.
    prefs_.game.present = true;
    prefs_.game.settings = sim_.challenge ? sim_.challenge->backup : sim_.settings;
    takerStats_[sim_.takerIndex] = {sim_.taker.accuracy, sim_.taker.curve, sim_.taker.power};
    prefs_.game.taker = takerStats_[sim_.takerIndex];
    prefs_.game.foot = sim_.foot;
    prefs_.game.runup = std::array<double, 2>{sim_.runup.angle, sim_.runup.dist};
    prefs_.game.showPath = sim_.showPath;
    ClientPrefs& c = prefs_.client;
    c.mode = sim_.mode;
    c.takerIndex = sim_.takerIndex;
    if (sim_.challenge) c.challengeIndex = sim_.challenge->index;
    c.difficulty = sim_.difficulty;
    savePrefs(prefsPath_, prefs_);
}

void App::selectTakerIdx(int i) {
    // Keep the edited stats of the taker we are leaving, and restore the ones of the taker we are picking.
    takerStats_[sim_.takerIndex] = {sim_.taker.accuracy, sim_.taker.curve, sim_.taker.power};
    sim::selectTaker(sim_, i);
    const auto& st = takerStats_[sim_.takerIndex];
    sim_.taker.accuracy = st[0];
    sim_.taker.curve = st[1];
    sim_.taker.power = st[2];
    sim_.previewKey.clear();
}

void App::resetAll() {
    sim_.settings = sim::defaultSettings();
    takerStats_ = takerDefaults_;
    sim_.taker.accuracy = takerDefaults_[sim_.takerIndex][0];
    sim_.taker.curve = takerDefaults_[sim_.takerIndex][1];
    sim_.taker.power = takerDefaults_[sim_.takerIndex][2];
    sim_.accuracyBonus = 0;
    sim_.difficulty = "normal";
    prefs_.client.difficulty = "normal";
    sim_.runup.angle = cfg_->shot.runupAngleDefault;
    sim_.runup.dist = cfg_->shot.runupDistDefault;
    sim_.showPath = false;
    sim_.previewKey.clear();
    sim::applySettings(sim_);
    persist();
}

void App::setPlacing(bool on) {
    placing_ = on && sim_.mode == sim::Mode::Practice;
    input_.placing = placing_;
}

void App::recordChallengeResult() {
    const auto& ch = sim_.challenge;
    if (!ch) {
        recorded_ = false;
        return;
    }
    if (!ch->done) {
        recorded_ = false;
        return;
    }
    if (recorded_) return;
    recorded_ = true;
    ClientPrefs& c = prefs_.client;
    const std::string& id = ch->def.id;
    auto b = c.best.find(id);
    if (b == c.best.end() || ch->points > b->second) c.best[id] = ch->points;
    if (ch->medal) {
        const std::string m = sim::str(*ch->medal);
        auto had = c.medals.find(id);
        if (had == c.medals.end() || medalRank(m) > medalRank(had->second)) c.medals[id] = m;
    }
    persist();
}

void App::screenshot(const std::string& path) {
    int w, h;
    window_.drawableSize(w, h);
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3), flipped(px.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; y++) std::copy_n(&px[static_cast<size_t>(h - 1 - y) * w * 3], static_cast<size_t>(w) * 3, &flipped[static_cast<size_t>(y) * w * 3]);
    stbi_write_png(path.c_str(), w, h, 3, flipped.data(), w * 3);
    std::printf("wrote %s (%dx%d)\n", path.c_str(), w, h);
}

int App::run() {
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running_ = false;

            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                if (sheets_.listening()) {
                    const std::string_view code = e.key.scancode == SDL_SCANCODE_ESCAPE ? "Escape" : platform::codeFor(e.key.scancode);
                    if (!code.empty()) sheets_.keyPressed(ctx_, std::string(code));
                    continue;  // never reaches the game
                }
                if (e.key.scancode == SDL_SCANCODE_F3) {
                    showStats_ = !showStats_;
                    continue;
                }
                if (e.key.scancode == SDL_SCANCODE_ESCAPE && sheets_.open != Sheet::None) {
                    sheets_.open = Sheet::None;
                    continue;
                }
            }
            // Mouse clicks on the UI must not capture the pointer or shoot.
            const bool mouseEvt = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP || e.type == SDL_EVENT_MOUSE_MOTION;
            if (mouseEvt && ImGui::GetIO().WantCaptureMouse && !input_.mouseLocked()) continue;
            input_.handleEvent(e, window_.handle());
        }

        const uint64_t now = SDL_GetPerformanceCounter();
        const double dt = std::min(MAX_FRAME, static_cast<double>(now - last_) / static_cast<double>(SDL_GetPerformanceFrequency()));
        last_ = now;
        frame(dt);
        if (opts_.bench > 0) {
            frameTimes_.push_back(dt * 1000);
            gpuSum_ += view_->gpuMs();
            if (static_cast<int>(frameTimes_.size()) >= opts_.bench) running_ = false;
        }
        if (opts_.frames > 0 && frameNo_ >= opts_.frames) running_ = false;
    }
    if (opts_.bench > 0 && frameTimes_.size() > 10) {
        // Skip the first frames (shader warm-up, IBL bake).
        std::vector<double> t(frameTimes_.begin() + 5, frameTimes_.end());
        std::sort(t.begin(), t.end());
        double sum = 0;
        for (double v : t) sum += v;
        int w, h;
        window_.drawableSize(w, h);
        std::printf("bench: %dx%d  quality=%s  msaa=%d  shadows=%s\n", w, h, qualityName(prefs_.client.quality), view_->msaaSamples(), view_->shadowsOn() ? "on" : "off");
        std::printf("bench: %zu frames  avg %.2f ms (%.0f fps)  p50 %.2f  p99 %.2f  max %.2f  | gpu avg %.2f ms\n", t.size(), sum / t.size(), 1000.0 * t.size() / sum,
                    t[t.size() / 2], t[static_cast<size_t>(t.size() * 0.99)], t.back(), gpuSum_ / frameTimes_.size());
    }
    return 0;
}

void App::frame(double frameDt) {
    ImGuiIO& io = ImGui::GetIO();
    ++frameNo_;
    smoothDt_ += (frameDt - smoothDt_) * 0.05;

    platform::Polled polled = input_.poll(frameDt);
    if (!paused_) {
        if (opts_.demoShoot) {
            // Scripted shot for screenshots: wait, hold shoot until the meter reaches the sweet spot, release.
            if (demoState_ == 0 && frameNo_ > 20) demoState_ = 1;
            if (demoState_ >= 1 && opts_.demoContact) {
                polled.input.contactX = opts_.demoContact->first;
                polled.input.contactY = opts_.demoContact->second;
            }
            if (demoState_ == 1) {
                polled.input.shoot = true;
                if (sim_.phase == sim::Phase::Charge && sim_.meter >= (cfg_->shot.sweetMin + cfg_->shot.sweetMax) / 2) demoState_ = 2;
            }
        }
        if (polled.cameraToggle) view_->rig.toggle();
        if (polled.placeToggle) setPlacing(!placing_);
        pendDX_ += polled.input.aimDeltaX;
        pendDY_ += polled.input.aimDeltaY;
        acc_ += frameDt;
        while (acc_ >= SIM_DT) {
            // Mouse movement is applied once, on the first step after it happened.
            sim::InputState in = polled.input;
            in.aimDeltaX = pendDX_;
            in.aimDeltaY = pendDY_;
            sim::step(sim_, in, SIM_DT);
            pendDX_ = pendDY_ = 0;
            acc_ -= SIM_DT;
        }
    }
    for (const auto& ev : sim_.events) view_->onSimEvent(ev);
    sim_.events.clear();
    recordChallengeResult();

    int fbW, fbH;
    window_.drawableSize(fbW, fbH);
    view_->render(sim_, acc_ / SIM_DT, paused_ ? 0 : frameDt, fbW, fbH);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    HudUi ui;
    ui.mouseAim = prefs_.client.mouseAim;
    ui.mouseLocked = input_.mouseLocked();
    ui.placing = placing_;
    ui.retryKey = platform::keyLabel(prefs_.client.keymap[static_cast<size_t>(platform::Action::Retry)][0]);
    ui.nextKey = platform::keyLabel(prefs_.client.keymap[static_cast<size_t>(platform::Action::NewSpot)][0]);
    hud_.draw(sim_, ui, *view_);
    const bool nowPaused = sheets_.draw(ctx_);
    if (nowPaused != paused_) {
        paused_ = nowPaused;
        input_.setEnabled(window_.handle(), !paused_);
        if (paused_) setPlacing(false);
    }
    if (showStats_) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "%.0f fps  %.2f ms | gpu %.2f ms | %dx%d  %dx MSAA  shadows %s", 1.0 / smoothDt_, smoothDt_ * 1000, view_->gpuMs(), fbW, fbH, view_->msaaSamples(),
                      view_->shadowsOn() ? "on" : "off");
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImVec2 sz = ImGui::CalcTextSize(buf);
        const ImVec2 p(io.DisplaySize.x - sz.x - 22, 70);
        dl->AddRectFilled(ImVec2(p.x - 8, p.y - 4), ImVec2(p.x + sz.x + 8, p.y + sz.y + 4), IM_COL32(0, 0, 0, 150), 6);
        dl->AddText(p, IM_COL32(255, 255, 255, 230), buf);
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (!opts_.shotPath.empty() && frameNo_ >= opts_.frames) screenshot(opts_.shotPath);
    window_.swap();
}

}  // namespace game
