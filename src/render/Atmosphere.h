#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "Common.h"
#include "Sphere.h"

// Build the atmosphere program and its interleaved VAO/VBO (positions followed
// by smooth normals) from the app's tetrahedron `planet` mesh -- the same mesh
// the planet bodies are drawn from.
void Atmosphere_InitGL(sphere mesh);

// Additive per-planet sky-dome (Rayleigh limb tint + white Mie term), centred on
// each planet's world translation (Planet_Transforms()). No depth write; the
// cull face is swapped to GL_FRONT while the camera sits inside the outer shell.
void Atmosphere_Draw(const ViewMatrices *vm);

#endif
