// Procedural mesh builders (position + normal), all in metres.
#pragma once
#include "gfx/gfx.h"

namespace render {

gfx::Mesh makeSphere(float radius, int slices = 24, int stacks = 16);
// Capsule along y centred at the origin; `length` is the cylindrical section (three.js CapsuleGeometry semantics).
gfx::Mesh makeCapsule(float radius, float length, int slices = 12, int stacks = 5);
gfx::Mesh makeCylinder(float radius, float height, int slices = 16);  // along y, centred
gfx::Mesh makeBox(float x, float y, float z);                          // centred
// Flat shapes lie in the XY plane facing +z.
gfx::Mesh makeDisc(float radius, int slices = 24);
gfx::Mesh makeRing(float inner, float outer, int slices = 48, float startAngle = 0, float arc = 6.2831853f);
gfx::Mesh makePlane(float w, float h);
// Polygon (convex or simple fan from first point) in XY.
gfx::Mesh makePolygon(const std::vector<glm::vec2>& pts);

}  // namespace render
