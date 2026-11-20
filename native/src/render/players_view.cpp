#include "render/players_view.h"

#include <array>

#include "anim/character_mesh.h"
#include "render/look.h"

namespace render {

using glm::vec3;

namespace {
struct Palette {
    std::array<vec3, anim::MATERIAL_COUNT> col;
    std::array<float, anim::MATERIAL_COUNT> rough;
};
const float ROUGH[anim::MATERIAL_COUNT] = {0.85f, 0.85f, 0.60f, 0.45f, 0.60f, 0.85f};
const unsigned SKINS[6] = {0xc68c63, 0x8d5a3b, 0xf2d0b5, 0xe8b894, 0x6b4430, 0xd9a577};

Palette paletteFor(const anim::Character& c) {
    Palette p;
    for (int i = 0; i < anim::MATERIAL_COUNT; i++) p.rough[i] = ROUGH[i];
    auto kit = [&](unsigned shirt, unsigned shorts, unsigned skin, unsigned gloves, unsigned socks) {
        p.col = {hex(shirt), hex(shorts), hex(skin), hex(0x111111), hex(gloves), hex(socks)};
    };
    switch (c.role) {
    case anim::Role::Taker: kit(0xd62828, 0xf4f4f4, 0xc68c63, 0xc68c63, 0xd62828); break;
    case anim::Role::Decoy: kit(0xd62828, 0xf4f4f4, 0xe8b894, 0xe8b894, 0xd62828); break;
    case anim::Role::Wall: {
        const unsigned skin = SKINS[c.variant % 6];
        kit(0x1f4fbf, 0x14233f, skin, skin, 0x1f4fbf);
        break;
    }
    case anim::Role::Lying: kit(0x1f4fbf, 0x14233f, SKINS[2], SKINS[2], 0x1f4fbf); break;
    case anim::Role::Keeper: kit(0xf2c200, 0x1c1c1c, 0xe8b894, 0xff5a1f, 0xf2c200); break;
    }
    return p;
}
}  // namespace

PlayersView::PlayersView() {
    const anim::SkinMesh& m = anim::characterMesh();
    std::vector<gfx::SkinnedVertex> v;
    v.reserve(m.vertices.size());
    for (const auto& sv : m.vertices) {
        gfx::SkinnedVertex g{};
        g.pos = sv.pos;
        g.normal = sv.normal;
        g.material = sv.material;
        for (int i = 0; i < 4; i++) {
            g.joints[i] = sv.joints[i];
            g.weights[i] = sv.weights[i];
        }
        v.push_back(g);
    }
    mesh_ = gfx::SkinnedMesh(v, m.indices);
}

void PlayersView::drawOne(const LitPainter& p, const anim::Character& c) const {
    const gfx::Shader& sh = *p.skinned;
    anim::BoneMats skin;
    anim::skinMatrices(c.pose, skin);
    sh.set("uModel", c.model);
    sh.setArray("uBones", skin.data(), anim::BONE_COUNT);
    if (!p.depthOnly) {
        const Palette pal = paletteFor(c);
        sh.setArray("uPalette", pal.col.data(), anim::MATERIAL_COUNT);
        sh.setArray("uPaletteRough", pal.rough.data(), anim::MATERIAL_COUNT);
    }
    mesh_.draw();
}

void PlayersView::draw(const LitPainter& p) const {
    p.skinned->use();
    drawOne(p, anim_.taker.character);
    if (anim_.decoy.character) drawOne(p, *anim_.decoy.character);
    for (const auto& c : anim_.wall.players) drawOne(p, c);
    if (anim_.wall.lying) drawOne(p, *anim_.wall.lying);
    if (anim_.keeper.character) drawOne(p, *anim_.keeper.character);
    p.shader->use();  // hand the static program back
}

}  // namespace render
