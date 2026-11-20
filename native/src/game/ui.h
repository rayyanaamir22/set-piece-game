// HUD (port of hud/Hud.ts) and the Settings / Controls sheets (hud/SettingsPanel.ts, ControlsPanel.ts),
// drawn with Dear ImGui. The HUD is custom-drawn on the background draw list; the sheets are ordinary ImGui windows.
#pragma once
#include <imgui.h>

#include <functional>
#include <string>

#include "game/prefs.h"
#include "render/game_view.h"
#include "sim/Sim.h"

namespace game {

// Things the HUD shows that live outside the sim.
struct HudUi {
    bool mouseAim = true;
    bool mouseLocked = false;
    bool placing = false;
    std::string retryKey, nextKey;
};

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
};
Fonts loadFonts(const std::string& assetDir);
void applyStyle();

class Hud {
public:
    void draw(const sim::SimState& s, const HudUi& ui, const render::GameView& view);

private:
    std::string lastToast_;
    double toastT_ = 0;
};

// Callbacks the sheets use to act on the running game.
struct SheetCtx {
    sim::SimState* sim = nullptr;
    ClientPrefs* prefs = nullptr;
    std::function<void()> apply;               // persist + push client prefs into the subsystems
    std::function<void()> onSimChange;         // persist after a sim setting changed
    std::function<void(sim::Mode)> setMode;
    std::function<void(int)> startChallenge;
    std::function<void(int)> selectTaker;
    std::function<void(sim::DifficultyName)> applyDifficulty;
    std::function<void()> togglePlacing;
    std::function<bool()> isPlacing;
    std::function<void()> resetAll;
    std::function<void(const platform::Keymap&)> setKeymap;
};

enum class Sheet { None, Controls, Settings };

class Sheets {
public:
    // Draws the menu buttons (always) and the open sheet. Returns true while a sheet is open (game paused).
    bool draw(SheetCtx& ctx);
    Sheet open = Sheet::None;
    // Called by the app with the next pressed key while a rebind is pending.
    bool listening() const { return listening_ >= 0; }
    void keyPressed(SheetCtx& ctx, const std::string& code);
    void cancelListening() { listening_ = -1; }

private:
    void drawControls(SheetCtx& ctx);
    void drawSettings(SheetCtx& ctx);
    int listening_ = -1;
    std::string message_;
    // Ball placement edited locally and applied with a button.
    float placeDistance_ = 25, placeAngle_ = 0;
    bool syncPlace_ = true;
};

}  // namespace game
