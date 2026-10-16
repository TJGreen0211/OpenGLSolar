#ifndef PLANET_H
#define PLANET_H

#include "Common.h"
#include "Sphere.h"

// Build the planet program and the shared interleaved VAO/VBO (positions,
// smooth normals, tangents) from the app's tetrahedron `planet` mesh.
void Planet_InitGL(sphere mesh);

// Draw all 11 planet bodies. Also writes each planet's world translation into
// the module's transform cache, which the ring/moon/atmosphere passes read via
// Planet_Transforms().
void Planet_Draw(const ViewMatrices *vm);

// Program + VAO + mesh-vertex-count, shared by the moon pass (same program).
GLuint Planet_Shader(void);
GLuint Planet_VAO(void);
int    Planet_MeshVertexCount(void);

// Per-planet world-translation matrices written by Planet_Draw (read by
// Rings/Moon/Atmosphere to place themselves at the planet centre).
const mat4 *Planet_Transforms(void);

void Planet_Shutdown(void);

#endif
