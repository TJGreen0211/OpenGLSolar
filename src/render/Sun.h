#ifndef SUN_H
#define SUN_H

#include "Common.h"

// Load the two sun maps (the app's "[trace] main: loading sun textures" slot);
// `step` advances the loading screen once per texture decode.
void Sun_InitTextures(void (*step)(void));

// Compile the sun billboard program and upload its fullscreen quad (the app's
// init(), run under the "init: tetrahedron" milestone).
void Sun_InitGL(void);

// Opaque sun sphere. Drawn BEFORE the planets with depth writes on, so its
// stored depth occludes any planet that orbits behind it (a closer planet
// still draws over the disc).
void Sun_DrawDisc(const ViewMatrices *vm);

// Additive corona halo. Drawn LAST: depth-tested against the scene (so a
// passing planet still hides it) but never writes depth, so planets behind the
// halo stay visible and the glow simply blends over them.
void Sun_DrawGlow(const ViewMatrices *vm);

#endif
