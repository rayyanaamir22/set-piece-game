#include <algorithm>
#include <cmath>
#include <cstdio>

#include "game/ui.h"
#include "platform/input.h"

namespace game {

using namespace sim;
using platform::Action;
using game::Quality;

namespace {
const ImVec4 DIMC(1, 1, 1, 0.62f), ACCENTC(1.f, 210 / 255.f, 63 / 255.f, 1);

void note(const std::string& t) {
    if (t.empty()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, DIMC);
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.86f);
    ImGui::TextWrapped("%s", t.c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// Row label, optionally with a "soon" badge (a feature that arrives in a later milestone).
void label(const char* text, bool soon = false) {
    ImGui::TextUnformatted(text);
    if (soon) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, DIMC);
        ImGui::TextUnformatted("[later milestone]");
        ImGui::PopStyleColor();
    }
}

// Segmented control; returns the newly chosen index, or -1.
int segmented(const char* id, const std::vector<std::string>& opts, int current) {
    int chosen = -1;
    ImGui::PushID(id);
    for (size_t i = 0; i < opts.size(); i++) {
        if (i) ImGui::SameLine(0, 2);
        const bool on = static_cast<int>(i) == current;
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENTC);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENTC);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1));
        }
        if (ImGui::Button((opts[i] + "##" + std::to_string(i)).c_str())) chosen = static_cast<int>(i);
        if (on) ImGui::PopStyleColor(3);
    }
    ImGui::PopID();
    return chosen;
}

bool sliderD(const char* id, double& v, double lo, double hi, const char* fmt) {
    float f = static_cast<float>(v);
    ImGui::SetNextItemWidth(-1);
    const bool ch = ImGui::SliderFloat(id, &f, static_cast<float>(lo), static_cast<float>(hi), fmt);
    if (ch) v = f;
    return ch;
}

bool sectionHeader(ClientPrefs& P, const char* title, bool collapsedDefault) {
    auto it = P.collapsed.find(title);
    const bool collapsed = it != P.collapsed.end() ? it->second : collapsedDefault;
    ImGui::SetNextItemOpen(!collapsed, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENTC);
    const bool open = ImGui::CollapsingHeader(title);
    ImGui::PopStyleColor();
    if (ImGui::IsItemToggledOpen()) P.collapsed[title] = !open;
    return open;
}

const char* PAD(Action a) {
    switch (a) {
    case Action::AimLeft: case Action::AimRight: case Action::AimUp: case Action::AimDown: return "Left stick";
    case Action::ContactLeft: case Action::ContactRight: case Action::ContactUp: case Action::ContactDown: return "Right stick";
    case Action::Shoot: return "B / Circle";
    case Action::Driven: return "RB / R1";
    case Action::Low: return "LB / L1";
    case Action::Dummy: return "L-stick click";
    case Action::RunupWider: return "RT / R2";
    case Action::RunupNarrower: return "LT / L2";
    case Action::RunupLonger: return "D-pad up";
    case Action::RunupShorter: return "D-pad down";
    case Action::ToggleFoot: return "Y / Triangle";
    case Action::NewSpot: return "X / Square";
    case Action::Retry: return "Start";
    case Action::Camera: return "Back / Select";
    case Action::Replay: return "R-stick click";
    default: return "";
    }
}

const char* NOTE(Action a) {
    switch (a) {
    case Action::AimLeft: return "Move the target reticle: where the ball should arrive. The arrow shows the launch direction.";
    case Action::ContactLeft: return "Side = curl · top = dip · bottom = lift · dead centre = knuckleball.";
    case Action::Shoot: return "Hold to charge, release in the green zone. Too early or late adds error; overcharging lifts it.";
    case Action::Driven: return "Faster and flatter, much less spin.";
    case Action::Low: return "A skidding ground drive: slips under a jumping wall, but a lying man stops it.";
    case Action::Dummy: return "Hold as you release: a decoy runs over the ball first and part of the wall jumps too early.";
    case Action::RunupWider: return "Straight = power, wide = curl. Past 45° a centre strike is no longer a knuckleball.";
    case Action::NewSpot: return "New random spot (free kicks / practice), or the next challenge when one is finished.";
    case Action::Retry: return "Skip the wait and retake, or restart a finished challenge.";
    case Action::Replay: return "Watch the last attempt from several cameras (arrives with the replay milestone).";
    case Action::PlaceBall: return "Practice mode: click the pitch to put the ball there.";
    default: return "";
    }
}

const char* medalIcon(const ClientPrefs& P, const std::string& id) {
    auto it = P.medals.find(id);
    return it == P.medals.end() ? "" : it->second.c_str();
}

void beginSheet(const char* name, const char* title, bool& closeRequested) {
    ImGuiIO& io = ImGui::GetIO();
    const float k = std::clamp(io.DisplaySize.y / 900.f, 0.85f, 1.8f);
    const float w = std::min(400.f * k, io.DisplaySize.x - 32), h = io.DisplaySize.y - 90 * k;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16 * k - w, 72 * k));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.2f);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, DIMC);
    ImGui::TextUnformatted("GAME PAUSED");
    ImGui::PopStyleColor();
    ImGui::SameLine(ImGui::GetWindowWidth() - 48);
    if (ImGui::Button("X", ImVec2(30, 0))) closeRequested = true;
    ImGui::Separator();
    ImGui::BeginChild("body", ImVec2(0, 0), false);
}
void endSheet() {
    ImGui::EndChild();
    ImGui::End();
}
}  // namespace

bool Sheets::draw(SheetCtx& ctx) {
    ImGuiIO& io = ImGui::GetIO();
    const float k = std::clamp(io.DisplaySize.y / 900.f, 0.85f, 1.8f);

    // Menu buttons (top right).
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16 * k, 16 * k), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("menubar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    auto menuBtn = [&](const char* t, Sheet s) {
        const bool on = open == s;
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENTC);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENTC);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(10 / 255.f, 16 / 255.f, 28 / 255.f, 0.62f));
        }
        if (ImGui::Button(t, ImVec2(96 * k, 40 * k))) {
            open = on ? Sheet::None : s;
            cancelListening();
            if (open == Sheet::Settings) syncPlace_ = true;
        }
        ImGui::PopStyleColor(on ? 3 : 1);
    };
    menuBtn("Controls", Sheet::Controls);
    ImGui::SameLine(0, 10 * k);
    menuBtn("Settings", Sheet::Settings);
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    bool close = false;
    if (open == Sheet::Controls) {
        beginSheet("Controls", "Controls", close);
        drawControls(ctx);
        endSheet();
    } else if (open == Sheet::Settings) {
        beginSheet("Settings", "Settings", close);
        drawSettings(ctx);
        endSheet();
    }
    if (close) {
        open = Sheet::None;
        cancelListening();
    }
    return open != Sheet::None;
}

void Sheets::keyPressed(SheetCtx& ctx, const std::string& code) {
    if (listening_ < 0) return;
    if (platform::isReserved(code)) {
        message_.clear();
        listening_ = -1;
        return;
    }
    const auto id = static_cast<platform::Action>(listening_);
    const platform::Keymap km = ctx.prefs->keymap;
    const auto clash = platform::conflicts(km, code, listening_);
    ctx.setKeymap(platform::rebind(km, id, code));
    if (!clash.empty()) {
        std::string names;
        for (size_t i = 0; i < clash.size(); i++) names += (i ? ", " : "") + std::string(platform::actionDef(clash[i]).label);
        message_ = platform::keyLabel(code) + " was " + names + ": that action took over " + platform::keyLabel(km[static_cast<size_t>(id)][0]) + ".";
    } else {
        message_.clear();
    }
    listening_ = -1;
}

void Sheets::drawControls(SheetCtx& ctx) {
    const platform::Keymap& km = ctx.prefs->keymap;
    if (!message_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ACCENTC);
        ImGui::TextWrapped("%s", message_.c_str());
        ImGui::PopStyleColor();
    }
    std::string group;
    for (const auto& a : platform::actions()) {
        if (a.group != group) {
            group = a.group;
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENTC);
            ImGui::TextUnformatted(group.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PushID(static_cast<int>(a.id));
        ImGui::TextUnformatted(a.label);
        const auto& keys = km[static_cast<size_t>(a.id)];
        float x = ImGui::GetWindowWidth() - 20;
        for (size_t i = 0; i < keys.size(); i++) x -= ImGui::CalcTextSize(platform::keyLabel(keys[i]).c_str()).x + 26;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8, x));
        for (size_t i = 0; i < keys.size(); i++) {
            if (i) {
                ImGui::SameLine(0, 4);
                ImGui::PushStyleColor(ImGuiCol_Text, DIMC);
                ImGui::TextUnformatted("or");
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 4);
            }
            const bool listen = listening_ == static_cast<int>(a.id) && i == 0;
            const std::string lbl = listen ? "press a key…" : platform::keyLabel(keys[i]);
            if (listen) {
                ImGui::PushStyleColor(ImGuiCol_Button, ACCENTC);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1));
            }
            ImGui::BeginDisabled(i != 0);
            if (ImGui::Button((lbl + "##k" + std::to_string(i)).c_str()) && i == 0) {
                listening_ = static_cast<int>(a.id);
                message_.clear();
            }
            ImGui::EndDisabled();
            if (listen) ImGui::PopStyleColor(2);
        }
        note(NOTE(a.id));
        if (*PAD(a.id)) {
            ImGui::PushStyleColor(ImGuiCol_Text, DIMC);
            const std::string pad = PAD(a.id);
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 24 - ImGui::CalcTextSize(pad.c_str()).x));
            ImGui::TextUnformatted(pad.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    auto table = [&](const char* title, std::vector<std::pair<const char*, const char*>> rows) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ACCENTC);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        for (auto& [what, how] : rows) {
            ImGui::TextUnformatted(what);
            ImGui::SameLine(ImGui::GetWindowWidth() - 20 - ImGui::CalcTextSize(how).x);
            ImGui::PushStyleColor(ImGuiCol_Text, DIMC);
            ImGui::TextUnformatted(how);
            ImGui::PopStyleColor();
        }
    };
    table("Mouse", {{"Capture the mouse", "Click the pitch"}, {"Move the reticle", "Move the mouse"}, {"Move the contact point", "Right button + move"},
                    {"Shoot (hold, release)", "Left button"}, {"Free the mouse / close menu", "Esc"}});
    ImGui::Spacing();
    if (ImGui::Button("Reset keys to defaults")) {
        listening_ = -1;
        message_.clear();
        ctx.setKeymap(platform::defaultKeymap());
    }
}

void Sheets::drawSettings(SheetCtx& ctx) {
    SimState& s = *ctx.sim;
    ClientPrefs& P = *ctx.prefs;
    SimSettings& st = s.settings;
    const SimConfig& cfg = *s.cfg;
    if (syncPlace_) {
        placeDistance_ = static_cast<float>(jhypot(s.spot.x, s.spot.z));
        placeAngle_ = static_cast<float>(std::atan2(s.spot.x, s.spot.z) * 180 / PI);
        syncPlace_ = false;
    }
    auto changed = [&] {
        applySettings(s);
        s.difficulty = "custom";  // hand-editing a rule means it is no longer one of the presets
        P.difficulty = "custom";
        ctx.onSimChange();
    };
    auto freq = [&](const char* name, Frequency& f, const char* help) {
        label(name);
        const int c = segmented(name, {"Off", "Sometimes", "Always"}, static_cast<int>(f));
        if (c >= 0) { f = static_cast<Frequency>(c); changed(); }
        note(help);
    };
    auto toggle = [&](const char* name, bool& v, const char* help, bool soon = false) {
        ImGui::BeginDisabled(soon);
        const bool ch = ImGui::Checkbox(name, &v);
        ImGui::EndDisabled();
        if (soon) { ImGui::SameLine(); ImGui::PushStyleColor(ImGuiCol_Text, DIMC); ImGui::TextUnformatted("[later milestone]"); ImGui::PopStyleColor(); }
        note(help);
        return ch;
    };
    auto slider = [&](const char* name, double& v, double lo, double hi, const char* fmt, const char* help, bool soon = false) {
        label(name, soon);
        ImGui::BeginDisabled(soon);
        const bool ch = sliderD((std::string("##") + name).c_str(), v, lo, hi, fmt);
        ImGui::EndDisabled();
        note(help);
        return ch;
    };

    // ---- Game mode
    if (sectionHeader(P, "Game mode", false)) {
        label("Mode");
        const int m = segmented("mode", {"Free kicks", "Practice", "Challenge"}, static_cast<int>(s.mode));
        if (m >= 0) ctx.setMode(static_cast<Mode>(m));
        note(s.mode == Mode::Free ? "A new spot every kick. Scored by goals."
             : s.mode == Mode::Practice ? "The ball stays where it is for instant retries. N picks a random spot; P places it with a click."
                                         : "Pick a challenge below: a fixed spot, rules and target rings. Gold needs most of the rings.");
        label("Challenge");
        std::vector<std::string> opts;
        for (size_t i = 0; i < cfg.challenges.size(); i++) opts.push_back(std::to_string(i + 1));
        const int c = segmented("challenge", opts, s.challenge ? s.challenge->index : -1);
        if (c >= 0) ctx.startChallenge(c);
        const ChallengeDef& cd = cfg.challenges[s.challenge ? s.challenge->index : P.challengeIndex % cfg.challenges.size()];
        char buf[256];
        std::snprintf(buf, sizeof buf, "%s: %s Medals %.0f / %.0f / %.0f.", cd.name.c_str(), cd.desc.c_str(), cd.medals.bronze, cd.medals.silver, cd.medals.gold);
        std::string n = buf;
        auto b = P.best.find(cd.id);
        if (b != P.best.end()) n += " Best " + std::to_string(static_cast<int>(b->second)) + " " + medalIcon(P, cd.id);
        note(n);
        if (ImGui::Button("Place ball with a click (P)")) { ctx.setMode(Mode::Practice); ctx.togglePlacing(); }
        ImGui::Spacing();
    }

    // ---- Difficulty
    if (sectionHeader(P, "Difficulty", false)) {
        label("Preset");
        std::vector<std::string> opts;
        int cur = -1;
        for (size_t i = 0; i < cfg.presets.size(); i++) {
            opts.push_back(cfg.presets[i].label);
            if (s.difficulty == DIFFICULTY_NAMES[i]) cur = static_cast<int>(i);
        }
        const int c = segmented("preset", opts, cur);
        if (c >= 0) ctx.applyDifficulty(static_cast<DifficultyName>(c));
        note("Sets the keeper, wall, wind and your accuracy together. Editing any of those afterwards makes it \"custom\".");
    }

    // ---- Taker
    if (sectionHeader(P, "Taker", false)) {
        label("Roster");
        std::vector<std::string> opts;
        for (size_t i = 0; i < cfg.takers.size(); i++) opts.push_back(std::to_string(i + 1));
        const int c = segmented("roster", opts, s.takerIndex);
        if (c >= 0) ctx.selectTaker(c);
        char buf[256];
        std::snprintf(buf, sizeof buf, "%s: %s, %s-footed. Accuracy %.2f, curve %.2f, power %.2f.", s.taker.name.c_str(), s.taker.style.c_str(),
                      s.taker.foot == 'R' ? "right" : "left", s.taker.accuracy, s.taker.curve, s.taker.power);
        note(buf);
    }

    // ---- Free kick
    if (sectionHeader(P, "Free kick", false)) {
        label("Distance to goal");
        { double d = placeDistance_; if (sliderD("##dist", d, 17, 40, "%.1f m")) placeDistance_ = static_cast<float>(d); }
        label("Angle");
        { double a = placeAngle_; if (sliderD("##ang", a, -60, 60, "%+.0f°")) placeAngle_ = static_cast<float>(a); }
        if (ImGui::Button("Place ball here") && s.phase == Phase::Aim) {
            const double a = placeAngle_ * PI / 180;
            setSpot(s, placeDistance_ * std::sin(a), placeDistance_ * std::cos(a));
            ctx.onSimChange();
        }
        ImGui::SameLine();
        if (ImGui::Button("Random spot") && s.phase == Phase::Aim) {
            const SpotXZ p = randomSpot(s);
            setSpot(s, p.x, p.z);
            syncPlace_ = true;
            ctx.onSimChange();
        }
        label("Taker foot");
        const int f = segmented("foot", {"Right", "Left"}, s.foot == 'R' ? 0 : 1);
        if (f >= 0) { s.foot = f == 0 ? 'R' : 'L'; ctx.onSimChange(); }
        ImGui::Spacing();
    }

    // ---- Run-up
    if (sectionHeader(P, "Run-up", true)) {
        if (slider("Approach angle", s.runup.angle, 0, cfg.shot.runupAngleMax, "%.0f°", "Straight = more power, less curl. Wide = more curl, less power. Past 45° a centre strike is no longer a knuckleball.")) ctx.onSimChange();
        if (slider("Run-up length", s.runup.dist, cfg.shot.runupDistMin, cfg.shot.runupDistMax, "%.1f m", "A longer run-up adds a little power.")) ctx.onSimChange();
        if (ImGui::Button("Reset run-up")) {
            s.runup.angle = cfg.shot.runupAngleDefault;
            s.runup.dist = cfg.shot.runupDistDefault;
            ctx.onSimChange();
        }
        ImGui::Spacing();
    }

    // ---- Defensive wall
    if (sectionHeader(P, "Defensive wall", false)) {
        if (toggle("Wall", st.wallEnabled, "")) changed();
        if (slider("Wall height", st.wallHeight, WALL_HEIGHT_MIN, WALL_HEIGHT_MAX, "%.2f m", "Taller walls force a higher, steeper dip.")) changed();
        label("Players in wall");
        int cnt = static_cast<int>(st.wallCount);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##wallcount", &cnt, 0, static_cast<int>(WALL_COUNT_MAX), cnt == 0 ? "Auto" : "%d")) { st.wallCount = cnt; changed(); }
        note("Auto: more players when closer and more central.");
        if (toggle("Wall jumps on the strike", st.wallJumps, "Jumpers raise the wall by about 0.4 m as the ball arrives, so go over with extra dip or round the end.")) changed();
    }

    // ---- Wall extras
    if (sectionHeader(P, "Wall extras", true)) {
        freq("Lying man", st.lyingMan, "A player lies behind the wall and stops low shots that slide under the jumpers.");
        freq("Encroachment", st.encroachment, "A wall player creeps forward before the kick. If the shot does not score, you retake it.");
    }

    // ---- Goalkeeper
    if (sectionHeader(P, "Goalkeeper", false)) {
        if (toggle("Goalkeeper present", st.keeperPresent, "")) changed();
        if (slider("Keeper skill", st.keeperSkill, 0, 1, "%.2f", "Reaction time, how well he reads curl, dive speed and how hard a shot he can hold. He covers the far post, cannot reach the top corners and never accounts for a knuckleball.")) changed();
        label("Keeper type");
        const int c = segmented("ktype", {"Balanced", "Quick", "Tall", "Strong"}, static_cast<int>(st.keeperType));
        if (c >= 0) { st.keeperType = static_cast<KeeperType>(c); changed(); }
        note("Quick: fast reactions, short reach. Tall: long reach, slow to react. Strong: holds harder shots but reads curl worse.");
        if (toggle("Rebounds", st.rebounds, "A parry that drops in the box becomes a follow-up shot.")) changed();
    }

    // ---- Conditions
    if (sectionHeader(P, "Conditions", true)) {
        if (toggle("Wind", st.windEnabled, "")) changed();
        if (slider("Wind speed", st.windSpeed, 0, WIND_SPEED_MAX, "%.1f m/s", "")) changed();
        if (slider("Wind direction", st.windDirection, 0, 360, "%.0f°", "0° blows towards goal, 90° towards the right. The aim solver ignores wind, so you have to allow for it; \"Show predicted path\" includes it.")) changed();
    }

    // ---- Taker tuning
    if (sectionHeader(P, "Taker tuning", true)) {
        auto tk = [&](const char* name, double& v, double lo, double hi) {
            if (slider(name, v, lo, hi, "%.2f", "")) { s.previewKey.clear(); ctx.onSimChange(); }
        };
        tk("Accuracy", s.taker.accuracy, 0, 1);
        tk("Curve", s.taker.curve, 0, 1.5);
        tk("Power", s.taker.power, 0.7, 1.2);
    }

    // ---- Aids
    if (sectionHeader(P, "Aids", true)) {
        if (toggle("Show predicted path", s.showPath, "Practice aid: draws the ideal flight of the current aim.")) { s.previewKey.clear(); ctx.onSimChange(); }
    }

    // ---- Input
    if (sectionHeader(P, "Input", true)) {
        if (toggle("Mouse aiming", P.mouseAim, "Click the pitch to capture the mouse: move = reticle, right button = contact point, left button = shoot. Esc frees it.")) ctx.apply();
        if (slider("Mouse sensitivity", P.mouseSensitivity, platform::SENS_MIN, platform::SENS_MAX, "%.2f×", "")) ctx.apply();
    }

    // ---- Audio (arrives with the audio milestone)
    if (sectionHeader(P, "Audio", true)) {
        if (slider("Master volume", P.masterVolume, 0, 1, "%.2f", "", true)) ctx.apply();
        if (slider("Crowd volume", P.crowdVolume, 0, 1, "%.2f", "", true)) ctx.apply();
        if (toggle("Mute", P.muted, "", true)) ctx.apply();
    }

    // ---- Graphics
    if (sectionHeader(P, "Graphics", true)) {
        label("Quality");
        const int qv = segmented("quality", {"Low", "Medium", "High"}, static_cast<int>(P.quality));
        if (qv >= 0) { P.quality = static_cast<Quality>(qv); ctx.apply(); }
        note("Low: no shadows, 2x anti-aliasing. Medium: 1K shadow maps. High: 2K shadow maps with soft edges.");
        label("Time of day");
        const int t = segmented("tod", {"Day", "Dusk", "Night"}, static_cast<int>(P.timeOfDay));
        if (t >= 0) { P.timeOfDay = static_cast<render::TimeOfDay>(t); ctx.apply(); }
        note("Sun, sky and exposure. Floodlights come with the stadium milestone.");
    }

    // ---- Camera
    if (sectionHeader(P, "Camera", true)) {
        if (slider("Camera distance", P.cameraDistance, 0.6, 1.8, "%.2f×", "How far behind the ball the camera sits (C still toggles near / far).")) ctx.apply();
    }

    // ---- Replay (arrives with the replay milestone)
    if (sectionHeader(P, "Replay", true)) {
        label("Replay speed", true);
        ImGui::BeginDisabled();
        int cur = 1;
        for (int i = 0; i < 3; i++) if (REPLAY_SPEEDS[i] == P.replaySpeed) cur = i;
        segmented("rspeed", {"0.25×", "0.5×", "1×"}, cur);
        ImGui::EndDisabled();
        note("Press V after a kick for an instant replay from several cameras.");
        toggle("Auto-replay goals", P.autoReplay, "", true);
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Reset all settings")) {
        ctx.resetAll();
        syncPlace_ = true;
    }
}

}  // namespace game
