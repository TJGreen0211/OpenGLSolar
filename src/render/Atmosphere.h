#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "Common.h"
#include "Sphere.h"

// Build the atmosphere program and its interleaved VAO/VBO (positions followed
// by smooth normals) from the app's tetrahedron `planet` mesh -- the same mesh
// the planet bodies are drawn from.
void Atmosphere_InitGL(sphere mesh);

// Additive per-planet sky-dome (Rayleigh limb tint + white Mie term), centred on
// each planet's world translation (Planet_Transforms()). No depth write; while
// the camera sits inside the outer shell the cull face swaps to GL_FRONT and
// depth testing is disabled, so the far hemisphere still blends its (surface-
// clipped) in-scatter over the planet. The planet pass applies the matching
// out-scatter extinction so fog is continuous across the shell boundary.
void Atmosphere_Draw(const ViewMatrices *vm);

#endif
