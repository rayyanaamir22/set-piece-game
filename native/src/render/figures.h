// Graybox players: capsule torso + head + two legs + two arms (port of render/Figures.ts).
#pragma once
#include <glm/glm.hpp>

#include "gfx/gfx.h"
#include "sim/Sim.h"

namespace render {

// Draws a lit mesh with a model matrix and an sRGB colour.
class LitPainter {
public:
    const gfx::Shader* shader = nullptr;
    void draw(const gfx::Mesh& m, const glm::mat4& model, const glm::vec3& srgb, float alpha = 1.f, int pattern = 0) const;
};

struct FigureColors {
    glm::vec3 shirt, shorts, skin;
};

struct FigurePose {
    float legA = 0, legB = 0;  // hip rotation about x (rad)
    glm::vec3 armAPos{0.3f, 1.3f, 0}, armBPos{-0.3f, 1.3f, 0};
    glm::vec3 armAEuler{0}, armBEuler{0};  // XYZ intrinsic, like three.js Euler
    bool gloves = false;
};

inline constexpr float FIGURE_HEIGHT = 1.92f;

class FigureMeshes {
public:
    FigureMeshes();
    // Faces +z. `model` places the figure's feet origin.
    void draw(const LitPainter& p, const glm::mat4& model, const FigureColors& c, const FigurePose& pose) const;

private:
    gfx::Mesh torso_, head_, leg_, foot_, arm_, glove_;
};

// The taker, the decoy runner, the wall, the lying man and the keeper.
class PlayerViews {
public:
    void draw(const LitPainter& p, const sim::SimState& s) const;

private:
    void drawKicker(const LitPainter& p, const sim::SimState& s) const;
    void drawDecoy(const LitPainter& p, const sim::SimState& s) const;
    void drawWall(const LitPainter& p, const sim::SimState& s) const;
    void drawKeeper(const LitPainter& p, const sim::SimState& s) const;
    FigureMeshes fig_;
};

}  // namespace render
