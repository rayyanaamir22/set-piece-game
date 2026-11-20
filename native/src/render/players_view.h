// Draws the animated players (taker, decoy, wall, lying man, keeper) with GPU skinning.
#pragma once
#include "anim/players.h"
#include "gfx/gfx.h"
#include "render/painter.h"

namespace render {

class PlayersView {
public:
    PlayersView();
    // Advance the animation (once per rendered frame).
    void update(const sim::SimState& s, double dt) { anim_.update(s, dt); }
    // Draw every character with `p.skinned` (the pass's skinned program, already carrying the pass uniforms).
    void draw(const LitPainter& p) const;
    const anim::PlayerAnimators& animators() const { return anim_; }

private:
    void drawOne(const LitPainter& p, const anim::Character& c) const;
    anim::PlayerAnimators anim_;
    gfx::SkinnedMesh mesh_;
};

}  // namespace render
