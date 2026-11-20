#include "golden_driver.h"

#include <algorithm>
#include <cmath>

#include "sim/config_io.h"

namespace testkit {

using json = nlohmann::json;
using namespace sim;

std::string repoRoot() { return SETPIECE_REPO_ROOT; }

std::shared_ptr<const SimConfig> repoConfig() {
    static std::shared_ptr<const SimConfig> cfg = loadConfig(repoRoot() + "/data");
    return cfg;
}

namespace {

json vec(const V3& a) { return json::array({a.x, a.y, a.z}); }

json resultJson(const std::optional<AttemptResult>& r) {
    if (!r) return nullptr;
    json j;
    j["type"] = str(r->type);
    j["woodwork"] = r->woodwork ? json(str(*r->woodwork)) : json(nullptr);
    j["wall"] = r->wall;
    j["keeper"] = r->keeper ? json(str(*r->keeper)) : json(nullptr);
    j["rebound"] = r->rebound;
    j["points"] = r->points;
    j["ring"] = r->ring;
    j["crossing"] = r->crossing ? json{{"x", r->crossing->x}, {"y", r->crossing->y}} : json(nullptr);
    return j;
}

json statsJson(const Stats& s) { return {{"attempts", s.attempts}, {"goals", s.goals}}; }

double num(const json& j, const char* k, double d) { return j.contains(k) && !j.at(k).is_null() ? j.at(k).get<double>() : d; }

}  // namespace

json runScenario(const json& fx, std::shared_ptr<const SimConfig> baseCfg) {
    const json& sc = fx.at("scenario");
    const SimSettings settings = parseSettings(fx.at("settings"));

    // The fixture carries the full resolved settings (so a port need not know BASE_SETTINGS).
    auto cfg = std::make_shared<SimConfig>(*baseCfg);
    SettingsPatch full;
    full.wallEnabled = settings.wallEnabled;
    full.wallHeight = settings.wallHeight;
    full.wallCount = settings.wallCount;
    full.wallJumps = settings.wallJumps;
    full.lyingMan = settings.lyingMan;
    full.encroachment = settings.encroachment;
    full.rebounds = settings.rebounds;
    full.keeperType = settings.keeperType;
    full.keeperPresent = settings.keeperPresent;
    full.keeperSkill = settings.keeperSkill;
    full.windEnabled = settings.windEnabled;
    full.windSpeed = settings.windSpeed;
    full.windDirection = settings.windDirection;
    cfg->settings = full;

    // Driver order matters: several calls draw from the RNG.
    SimState s = createSim(cfg, static_cast<int32_t>(sc.at("seed").get<double>()));
    if (sc.contains("challenge")) startChallenge(s, sc.at("challenge").get<int>());
    selectTaker(s, sc.contains("taker") ? sc.at("taker").get<int>() : 0);
    if (sc.contains("foot")) s.foot = sc.at("foot").get<std::string>().at(0);
    const json& spot = sc.at("spot");
    if (!sc.contains("challenge")) setSpot(s, spot.at("x"), spot.at("z"));
    s.aim = {sc.at("aim").at("x"), sc.at("aim").at("y")};
    s.contact = {sc.at("contact").at("x"), sc.at("contact").at("y")};
    if (sc.contains("runup")) s.runup = {sc.at("runup").at("angle"), sc.at("runup").at("dist")};

    const bool driven = sc.value("driven", false), low = sc.value("low", false), dummy = sc.value("dummy", false);
    InputState idle = emptyInput();
    idle.driven = driven;
    idle.low = low;
    idle.contactX = sc.at("contact").at("x");
    idle.contactY = sc.at("contact").at("y");
    InputState hold = idle;
    hold.shoot = true;
    InputState release = idle;
    release.dummy = dummy;

    json samples = json::array();
    int n = 0;
    auto run = [&](const InputState& in) {
        step(s, in, DT);
        n++;
        if (n % SAMPLE_EVERY == 0) {
            json smp;
            smp["step"] = n;
            smp["phase"] = str(s.phase);
            smp["ball"] = vec(s.ball.pos);
            smp["vel"] = vec(s.ball.vel);
            smp["spin"] = vec(s.ball.spin);
            smp["keeperHand"] = s.keeper ? json::array({s.keeper->b.x, s.keeper->b.y}) : json(nullptr);
            smp["wallLiftMax"] = s.wall ? json(*std::max_element(s.wall->lift.begin(), s.wall->lift.end())) : json(nullptr);
            samples.push_back(smp);
        }
    };

    const double power = sc.at("power");
    int holdSteps = 0, releaseStep = 0;
    if (sc.contains("force")) {
        const json& f = sc.at("force");
        s.phase = Phase::Flight;
        if (s.keeper) s.keeper->phase = KeeperPhase::Watching;
        s.ball.pos = {f.at("ball").at("x"), f.at("ball").at("y"), f.at("ball").at("z")};
        s.ball.vel = {f.at("vel").at("x"), f.at("vel").at("y"), f.at("vel").at("z")};
        s.prevBallPos = s.ball.pos;
    } else {
        run(hold);
        while (s.meter < power) run(hold);
        holdSteps = n;
        run(release);
        releaseStep = n;
    }
    while (n < 240 * 10 && s.phase != Phase::Result) run(idle);
    const json atResult = {{"result", resultJson(s.result)}, {"stats", statsJson(s.stats)}};
    if (sc.value("continueAfterResult", false))
        while (n < 240 * 20 && s.phase == Phase::Result) run(idle);

    json fin;
    fin["phase"] = str(s.phase);
    fin["atResult"] = atResult;
    fin["result"] = resultJson(s.result);
    fin["stats"] = statsJson(s.stats);
    fin["rng"] = s.rng.s;
    fin["ball"] = {{"pos", vec(s.ball.pos)}, {"vel", vec(s.ball.vel)}, {"spin", vec(s.ball.spin)}};
    fin["flightT"] = s.flightT;
    fin["encroached"] = s.encroached;
    fin["crossing"] = s.crossing ? json{{"x", s.crossing->x}, {"y", s.crossing->y}} : json(nullptr);
    fin["spot"] = vec(s.spot);
    fin["spotId"] = s.spotId;
    fin["followUp"] = s.followUp;
    fin["reboundCount"] = s.reboundCount;
    fin["keeperTouch"] = s.keeperTouch ? json(str(*s.keeperTouch)) : json(nullptr);
    fin["woodwork"] = s.woodwork ? json(str(*s.woodwork)) : json(nullptr);
    fin["hitWall"] = s.hitWall;
    if (s.challenge) {
        fin["challenge"] = {{"taken", s.challenge->taken},
                            {"points", s.challenge->points},
                            {"done", s.challenge->done},
                            {"medal", s.challenge->medal ? json(str(*s.challenge->medal)) : json(nullptr)}};
    } else {
        fin["challenge"] = nullptr;
    }

    json out;
    out["inputs"] = {{"holdSteps", holdSteps}, {"releaseStep", releaseStep}, {"totalSteps", n}};
    out["samples"] = samples;
    out["final"] = fin;
    (void)num;
    return out;
}

namespace {

// Keys whose numeric values are integers or RNG state: they must match exactly.
bool exactKey(const std::string& k) {
    static const char* keys[] = {"rng", "step", "holdSteps", "releaseStep", "totalSteps", "spotId", "reboundCount",
                                 "attempts", "goals", "taken", "points", "ring"};
    return std::any_of(std::begin(keys), std::end(keys), [&](const char* x) { return k == x; });
}

std::optional<std::string> diff(const json& a, const json& b, const std::string& path, const std::string& key) {
    if (a.is_number() && b.is_number()) {
        const double x = a.get<double>(), y = b.get<double>();
        double tol;
        if (exactKey(key)) tol = 0;
        else if (key == "vel" || key == "spin") tol = 1e-3 * std::max(1.0, std::fabs(y));
        else if (key == "flightT") tol = 1e-6;
        else tol = 0.01;  // positions: 1 cm
        if (std::fabs(x - y) <= tol) return std::nullopt;
        return path + ": " + std::to_string(x) + " vs " + std::to_string(y) + " (tol " + std::to_string(tol) + ")";
    }
    if (a.type() != b.type()) return path + ": " + a.dump() + " vs " + b.dump();
    if (a.is_object()) {
        if (a.size() != b.size()) return path + ": keys differ";
        for (auto it = b.begin(); it != b.end(); ++it) {
            if (!a.contains(it.key())) return path + "." + it.key() + ": missing";
            if (auto d = diff(a.at(it.key()), it.value(), path + "." + it.key(), it.key())) return d;
        }
        return std::nullopt;
    }
    if (a.is_array()) {
        if (a.size() != b.size()) return path + ": length " + std::to_string(a.size()) + " vs " + std::to_string(b.size());
        for (size_t i = 0; i < a.size(); i++)
            if (auto d = diff(a[i], b[i], path + "[" + std::to_string(i) + "]", key)) return d;
        return std::nullopt;
    }
    return a == b ? std::nullopt : std::optional<std::string>(path + ": " + a.dump() + " vs " + b.dump());
}

}  // namespace

std::optional<std::string> firstDiff(const json& got, const json& want) { return diff(got, want, "$", ""); }

}  // namespace testkit
