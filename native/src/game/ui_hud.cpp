#include <algorithm>
#include <cmath>
#include <cstdio>

#include "game/ui.h"
#include "sim/layout.h"

namespace game {

using sim::Phase;

namespace {
Fonts g_fonts;
float g_scale = 1;

constexpr ImU32 col(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }
const ImU32 FG = col(255, 255, 255), DIM = col(255, 255, 255, 178), ACCENT = col(255, 210, 63), GOOD = col(70, 211, 105), BAD = col(255, 123, 123);
const ImU32 PANEL = col(10, 16, 28, 158);

struct Span {
    std::string text;
    bool bold = false;
    ImU32 color = DIM;
};

float textW(ImFont* f, float size, const std::string& t) { return f->CalcTextSizeA(size, 1e9f, 0.f, t.c_str()).x; }

float drawSpans(ImDrawList* dl, ImVec2 p, float size, const std::vector<Span>& spans) {
    float x = p.x;
    for (const auto& s : spans) {
        ImFont* f = s.bold ? g_fonts.bold : g_fonts.regular;
        dl->AddText(f, size, ImVec2(x, p.y), s.color, s.text.c_str());
        x += textW(f, size, s.text);
    }
    return x - p.x;
}
float spansW(float size, const std::vector<Span>& spans) {
    float w = 0;
    for (const auto& s : spans) w += textW(s.bold ? g_fonts.bold : g_fonts.regular, size, s.text);
    return w;
}

std::string fmt(const char* f, double v) {
    char b[64];
    std::snprintf(b, sizeof b, f, v);
    return b;
}
}  // namespace

Fonts loadFonts(const std::string& dir) {
    ImGuiIO& io = ImGui::GetIO();
    g_fonts.regular = io.Fonts->AddFontFromFileTTF((dir + "/fonts/Roboto-Regular.ttf").c_str(), 16.f);
    g_fonts.bold = io.Fonts->AddFontFromFileTTF((dir + "/fonts/Roboto-Bold.ttf").c_str(), 16.f);
    if (!g_fonts.regular) g_fonts.regular = io.Fonts->AddFontDefault();
    if (!g_fonts.bold) g_fonts.bold = g_fonts.regular;
    io.FontDefault = g_fonts.regular;
    return g_fonts;
}

void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark(&s);
    s.WindowRounding = 14;
    s.FrameRounding = 8;
    s.GrabRounding = 8;
    s.PopupRounding = 8;
    s.ChildRounding = 8;
    s.ScrollbarRounding = 8;
    s.WindowBorderSize = 1;
    s.FrameBorderSize = 0;
    s.ItemSpacing = ImVec2(8, 7);
    s.WindowPadding = ImVec2(16, 12);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(12 / 255.f, 18 / 255.f, 32 / 255.f, 0.94f);
    c[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.12f);
    c[ImGuiCol_Text] = ImVec4(1, 1, 1, 1);
    c[ImGuiCol_TextDisabled] = ImVec4(1, 1, 1, 0.62f);
    c[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 0.10f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1, 1, 1, 0.16f);
    c[ImGuiCol_FrameBgActive] = ImVec4(1, 1, 1, 0.22f);
    c[ImGuiCol_SliderGrab] = ImVec4(1.f, 210 / 255.f, 63 / 255.f, 1);
    c[ImGuiCol_SliderGrabActive] = ImVec4(1.f, 225 / 255.f, 120 / 255.f, 1);
    c[ImGuiCol_CheckMark] = ImVec4(70 / 255.f, 211 / 255.f, 105 / 255.f, 1);
    c[ImGuiCol_Button] = ImVec4(1, 1, 1, 0.12f);
    c[ImGuiCol_ButtonHovered] = ImVec4(1, 1, 1, 0.22f);
    c[ImGuiCol_ButtonActive] = ImVec4(1, 1, 1, 0.30f);
    c[ImGuiCol_Header] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_HeaderHovered] = ImVec4(1, 1, 1, 0.06f);
    c[ImGuiCol_HeaderActive] = ImVec4(1, 1, 1, 0.10f);
    c[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = ImVec4(0, 0, 0, 0);
}

void Hud::draw(const sim::SimState& s, const HudUi& ui, const render::GameView& view) {
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    g_scale = std::clamp(H / 900.f, 0.85f, 1.8f);
    const float k = g_scale;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto panel = [&](ImVec2 a, ImVec2 b) { dl->AddRectFilled(a, b, PANEL, 10 * k); };

    // ---- top-left: distance, mode, taker, stats, run-up, wind
    {
        const float x = 16 * k, pad = 14 * k;
        float y = 16 * k + 10 * k;
        const double dist = sim::jhypot(s.spot.x, s.spot.z);
        const std::string distS = fmt("%.0f", std::round(dist)) + " m";
        const std::string modeS = s.mode == sim::Mode::Free ? "Free kicks" : s.mode == sim::Mode::Practice ? "Practice" : "Challenge";
        const sim::RunupEffect fx = sim::runupEffect(sim::RunupSetting{s.runup.angle, s.runup.dist}, s.cfg->shot);
        auto pct = [](double m) { return std::string(m >= 1 ? "+" : "−") + fmt("%.0f", std::round(std::fabs(m - 1) * 100)) + "%"; };
        const std::string fxS = "power " + pct(fx.power) + ", curl " + pct(fx.curl) + (fx.knuckleOk ? "" : ", no knuckle");
        const sim::V3 wv = sim::windVector(s.settings);
        const double wspeed = sim::jhypot(wv.x, wv.z);

        const float line = 13 * k, gap = 5 * k, bigS = 34 * k;
        float h = bigS + gap + 5 * (line + gap) + (wspeed > 0 ? line + gap : 0) + 2 * 8 * k;
        const float w = 230 * k;
        panel(ImVec2(x, 16 * k), ImVec2(x + w, 16 * k + h));
        dl->AddText(g_fonts.bold, bigS, ImVec2(x + pad, y - 4 * k), FG, distS.c_str());
        y += bigS + gap;
        drawSpans(dl, ImVec2(x + pad, y), line, {{modeS, true, FG}, {" · " + s.difficulty, false, DIM}});
        y += line + gap;
        std::vector<Span> taker = {{s.taker.name, true, FG}, {std::string(" · ") + (s.foot == 'R' ? "right" : "left") + "-footed", false, DIM}};
        if (s.followUp) taker.push_back({" · Rebound!", true, ACCENT});
        drawSpans(dl, ImVec2(x + pad, y), line, taker);
        y += line + gap;
        drawSpans(dl, ImVec2(x + pad, y), line, {{"Goals ", false, DIM}, {std::to_string(s.stats.goals), true, FG}, {" / " + std::to_string(s.stats.attempts), false, DIM}});
        y += line + gap;
        drawSpans(dl, ImVec2(x + pad, y), line, {{"Run-up ", false, DIM}, {fmt("%.0f", std::round(s.runup.angle)) + "°", true, FG}, {" · ", false, DIM}, {fmt("%.1f", s.runup.dist) + " m", true, FG}});
        y += line + 1 * k;
        dl->AddText(g_fonts.regular, 12 * k, ImVec2(x + pad, y), DIM, fxS.c_str());
        y += 12 * k + gap;
        if (wspeed > 0) {
            // Arrow relative to the player's view: up = blowing towards goal.
            const sim::V3 f = sim::dirToGoal(s.spot), r = sim::rightOf(f);
            const double up = wv.x * f.x + wv.z * f.z, right = wv.x * r.x + wv.z * r.z;
            const float ang = static_cast<float>(std::atan2(right, up));
            const ImVec2 c(x + pad + 8 * k, y + 7 * k);
            auto rot = [&](float px, float py) { return ImVec2(c.x + px * std::cos(ang) - py * std::sin(ang), c.y + px * std::sin(ang) + py * std::cos(ang)); };
            const float a = 7 * k;
            dl->AddTriangleFilled(rot(0, -a), rot(a * 0.9f, a * 0.35f), rot(-a * 0.9f, a * 0.35f), ACCENT);
            dl->AddQuadFilled(rot(-a * 0.3f, a * 0.2f), rot(a * 0.3f, a * 0.2f), rot(a * 0.3f, a), rot(-a * 0.3f, a), ACCENT);
            const char* kind = std::fabs(up) > std::fabs(right) * 1.7 ? (up > 0 ? "tailwind" : "headwind") : std::fabs(right) > std::fabs(up) * 1.7 ? "crosswind" : "quartering";
            drawSpans(dl, ImVec2(x + pad + 22 * k, y), line, {{"Wind ", false, DIM}, {fmt("%.1f", wspeed) + " m/s", true, FG}, {std::string(" ") + kind, false, DIM}});
        }
    }

    // ---- power meter
    {
        const bool charging = s.phase == Phase::Charge;
        const bool show = s.phase == Phase::Aim || charging;
        if (show) {
            const float w = std::min(420.f * k, W * 0.6f), h = 18 * k;
            const float x = (W - w) / 2, y = H - 28 * k - 40 * k;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col(0, 0, 0, 140), h / 2);
            const float fillW = static_cast<float>((charging ? s.meter : 0) * w);
            if (fillW > 1) {
                dl->PushClipRect(ImVec2(x, y), ImVec2(x + fillW, y + h), true);
                dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), col(74, 163, 255), col(255, 210, 63), col(255, 210, 63), col(74, 163, 255));
                dl->PopClipRect();
            }
            const float sx = x + static_cast<float>(s.cfg->shot.sweetMin) * w, sw = static_cast<float>(s.cfg->shot.sweetMax - s.cfg->shot.sweetMin) * w;
            dl->AddRectFilled(ImVec2(sx, y), ImVec2(sx + sw, y + h), col(70, 211, 105, 140));
            dl->AddLine(ImVec2(sx, y), ImVec2(sx, y + h), GOOD, 2);
            dl->AddLine(ImVec2(sx + sw, y), ImVec2(sx + sw, y + h), GOOD, 2);
            dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col(255, 255, 255, 128), h / 2, 2.0f, ImDrawFlags_None);
            const char* label = charging ? (s.meter > s.cfg->shot.sweetMax ? "Overpowered!" : s.meter >= s.cfg->shot.sweetMin ? "NOW!" : "Charging…") : "Hold SPACE to charge";
            const float ls = 12 * k;
            const float lw = textW(g_fonts.regular, ls, label);
            dl->AddText(g_fonts.regular, ls, ImVec2((W - lw) / 2, y + h + 6 * k), DIM, label);
        }
    }

    // ---- contact widget
    {
        const float R = 56 * k;
        const ImVec2 c(W - 18 * k - 75 * k, H - 18 * k - 32 * k - R * 2 + R);
        const bool knuckle = s.preview && s.preview->knuckle;
        panel(ImVec2(c.x - 75 * k, c.y - R - 12 * k), ImVec2(c.x + 75 * k, H - 18 * k));
        dl->AddCircleFilled(c, R, col(214, 214, 214), 48);
        dl->AddCircleFilled(ImVec2(c.x - R * 0.25f, c.y - R * 0.25f), R * 0.55f, col(255, 255, 255, 120), 32);
        dl->AddLine(ImVec2(c.x - R, c.y), ImVec2(c.x + R, c.y), col(0, 0, 0, 90), 1.5f);
        dl->AddLine(ImVec2(c.x, c.y - R), ImVec2(c.x, c.y + R), col(0, 0, 0, 90), 1.5f);
        dl->AddCircle(c, R * 0.18f, col(0, 0, 0, 90), 24, 1.5f);
        const ImVec2 d(c.x + static_cast<float>(s.contact.x) * R, c.y - static_cast<float>(s.contact.y) * R);
        dl->AddCircleFilled(d, 6 * k, knuckle ? col(255, 90, 90) : col(42, 123, 255), 20);
        dl->AddCircle(d, 6 * k, FG, 20, 2);

        // Curl description.
        std::vector<std::vector<Span>> lines;
        const double cx = s.contact.x, cy = s.contact.y;
        if (knuckle) lines = {{{"Knuckleball", true, FG}}, {{"unpredictable late movement", false, DIM}}};
        else {
            std::vector<Span> a;
            if (std::fabs(cx) > 0.12) a = {{"curls ", false, DIM}, {cx > 0 ? "left" : "right", true, FG}, {" " + fmt("%.0f", std::round(std::fabs(cx) * 100)) + "%", false, DIM}};
            std::vector<Span> b;
            if (std::fabs(cy) > 0.12) b = {{cy > 0 ? "dips" : "lifts", true, FG}, {" " + fmt("%.0f", std::round(std::fabs(cy) * 100)) + "%", false, DIM}};
            if (!a.empty()) lines.push_back(a);
            if (!b.empty()) lines.push_back(b);
            if (lines.empty()) lines = {{{"Straight", true, FG}}, {{"centre contact", false, DIM}}};
        }
        const std::string mods = std::string(s.driven ? "driven" : "") + (s.driven && s.low ? " + " : "") + (s.low ? "low" : "");
        if (!mods.empty()) lines.push_back({{mods, false, DIM}});
        float ty = c.y + R + 8 * k;
        for (const auto& ln : lines) {
            const float w = spansW(12 * k, ln);
            drawSpans(dl, ImVec2(c.x - w / 2, ty), 12 * k, ln);
            ty += 14 * k;
        }
    }

    // ---- challenge bar + end-of-run summary
    const auto& ch = s.challenge;
    if (ch) {
        const auto& m = ch->def.medals;
        const double shown = std::min(ch->taken + ((s.phase == Phase::Result || ch->done) ? 0 : 1), ch->def.shots);
        std::vector<Span> sp = {{ch->def.name, true, FG}, {" · shot ", false, DIM}, {fmt("%.0f", shown) + "/" + fmt("%.0f", ch->def.shots), true, FG}, {" · ", false, DIM},
                                {fmt("%.0f", ch->points), true, FG}, {" pts   ", false, DIM},
                                {"bronze " + fmt("%.0f", m.bronze) + "  silver " + fmt("%.0f", m.silver) + "  gold " + fmt("%.0f", m.gold), false, DIM}};
        const float w = spansW(14 * k, sp) + 28 * k;
        panel(ImVec2((W - w) / 2, 16 * k), ImVec2((W + w) / 2, 16 * k + 38 * k));
        drawSpans(dl, ImVec2((W - w) / 2 + 14 * k, 16 * k + 10 * k), 14 * k, sp);
        if (ch->done) {
            const float pw = 280 * k, ph = (150 + 22 * ch->log.size()) * k;
            const ImVec2 a((W - pw) / 2, H * 0.26f);
            panel(a, ImVec2(a.x + pw, a.y + ph));
            const char* mname = ch->medal ? sim::str(*ch->medal) : nullptr;
            std::string title = mname ? std::string(1, static_cast<char>(std::toupper(mname[0]))) + (mname + 1) + " medal" : "No medal";
            const float tw = textW(g_fonts.bold, 24 * k, title);
            dl->AddText(g_fonts.bold, 24 * k, ImVec2(a.x + (pw - tw) / 2, a.y + 14 * k), ch->medal == sim::Medal::Gold ? ACCENT : FG, title.c_str());
            const std::string pts = fmt("%.0f", ch->points) + " points";
            const float pw2 = textW(g_fonts.regular, 16 * k, pts);
            dl->AddText(g_fonts.regular, 16 * k, ImVec2(a.x + (pw - pw2) / 2, a.y + 46 * k), DIM, pts.c_str());
            float ly = a.y + 76 * k;
            int i = 1;
            for (const auto& l : ch->log) {
                std::string what = l.type == sim::ResultType::Goal ? (l.ring >= 0 ? "ring " + std::to_string(l.ring + 1) : "goal") : sim::str(l.type);
                dl->AddText(g_fonts.regular, 13 * k, ImVec2(a.x + 18 * k, ly), DIM, (std::to_string(i++) + "   " + what).c_str());
                const std::string p = fmt("%.0f", l.points);
                dl->AddText(g_fonts.bold, 13 * k, ImVec2(a.x + pw - 18 * k - textW(g_fonts.bold, 13 * k, p), ly), FG, p.c_str());
                ly += 22 * k;
            }
            const std::string hint = "[" + ui.retryKey + "] try again   [" + ui.nextKey + "] next challenge";
            dl->AddText(g_fonts.regular, 12 * k, ImVec2(a.x + (pw - textW(g_fonts.regular, 12 * k, hint)) / 2, ly + 10 * k), DIM, hint.c_str());
        }
    }

    // ---- target ring labels
    for (const auto& r : view.ringLabels) {
        const auto sp = view.project(r.pos);
        if (!sp.visible) continue;
        const std::string t = fmt("%.0f", r.points);
        const float size = std::max(14.f, r.size * 70.f * k * (H / 900.f) * 0.6f);
        const float w = textW(g_fonts.bold, size, t);
        const ImVec2 p(sp.px.x / io.DisplayFramebufferScale.x - w / 2, sp.px.y / io.DisplayFramebufferScale.y - size / 2);
        dl->AddText(g_fonts.bold, size, ImVec2(p.x + 1.5f, p.y + 1.5f), col(0, 0, 0, 150), t.c_str());
        dl->AddText(g_fonts.bold, size, p, FG, t.c_str());
    }

    // ---- result toast
    {
        const bool done = ch && ch->done;
        std::string key, title, sub;
        ImU32 color = FG;
        if (s.phase == Phase::Result && s.result && !done) {
            const auto& r = s.result;
            const char* wood = r->woodwork == sim::Woodwork::Post ? "the post" : r->woodwork == sim::Woodwork::Bar ? "the crossbar" : "";
            using R = sim::ResultType;
            if (r->type == R::Goal) {
                title = "GOAL!";
                sub = r->woodwork ? std::string("Off ") + wood + " and in" : r->keeper ? "Through the keeper's hands" : r->wall ? "Deflected off the wall" : "";
                if (s.challenge && r->points > 0) sub += std::string(sub.empty() ? "" : " · ") + "+" + fmt("%.0f", r->points) + " pts" + (r->ring >= 0 ? " (ring " + std::to_string(r->ring + 1) + ")" : "");
                color = GOOD;
            } else if (r->type == R::Retake) {
                title = "Retake!";
                sub = "The wall encroached";
                color = ACCENT;
            } else if (r->type == R::Saved) {
                title = "Saved!";
                sub = r->keeper == sim::KeeperTouch::Hold ? "The keeper holds it" : r->rebound ? "Spilled in the box: rebound!" : "Pushed away";
                color = r->rebound ? ACCENT : BAD;
            } else if (r->woodwork) {
                title = r->woodwork == sim::Woodwork::Bar ? "Off the bar!" : "Off the post!";
                sub = "So close";
                color = ACCENT;
            } else if (r->type == R::Miss && r->wall) {
                title = "Blocked!";
                sub = "Hit the wall";
                color = BAD;
            } else if (r->type == R::Over) {
                title = "Over the bar";
                sub = r->wall ? "Deflected off the wall" : "";
                color = BAD;
            } else if (r->type == R::Wide) {
                title = "Wide";
                sub = r->wall ? "Deflected off the wall" : "";
                color = BAD;
            } else {
                title = "Short";
                color = BAD;
            }
            key = std::to_string(s.stats.attempts) + ":" + title;
        }
        if (key != lastToast_) {
            lastToast_ = key;
            toastT_ = 0;
        }
        toastT_ += io.DeltaTime;
        if (!key.empty()) {
            const float a = static_cast<float>(std::min(1.0, toastT_ / 0.18));
            const float sc = (0.9f + 0.1f * a);
            const float size = std::clamp(H * 0.085f, 34.f, 76.f) * sc;
            const float tw = textW(g_fonts.bold, size, title);
            const ImVec2 p((W - tw) / 2, H * 0.18f);
            const ImU32 c = (color & 0x00FFFFFF) | (static_cast<ImU32>(255 * a) << 24);
            dl->AddText(g_fonts.bold, size, ImVec2(p.x + 2, p.y + 4), col(0, 0, 0, static_cast<int>(120 * a)), title.c_str());
            dl->AddText(g_fonts.bold, size, p, c, title.c_str());
            if (!sub.empty()) {
                const float ss = size * 0.32f;
                dl->AddText(g_fonts.regular, ss, ImVec2((W - textW(g_fonts.regular, ss, sub)) / 2, p.y + size + 4 * k), (DIM & 0x00FFFFFF) | (static_cast<ImU32>(178 * a) << 24), sub.c_str());
            }
        }
    }

    // ---- one-line hints about the mouse / placing the ball
    {
        std::string hint;
        if (ui.placing) hint = "Click the pitch to place the ball · P to cancel";
        else if (ui.mouseAim && !ui.mouseLocked && s.phase == Phase::Aim) hint = "Click the pitch to aim with the mouse";
        else if (ui.mouseLocked) hint = "Mouse captured · Esc to release";
        if (!hint.empty()) {
            const float size = 13 * k, w = textW(g_fonts.regular, size, hint) + 28 * k;
            const float x = 16 * k, y = H - 16 * k - 30 * k;
            panel(ImVec2(x, y), ImVec2(x + w, y + 30 * k));
            dl->AddText(g_fonts.regular, size, ImVec2(x + 14 * k, y + 8 * k), DIM, hint.c_str());
        }
    }
}

}  // namespace game
