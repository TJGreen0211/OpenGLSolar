#ifndef RINGS_H
#define RINGS_H

#include "Common.h"

// Compile the ring program, synthesize each ring's radial-band texture, build
// the annulus VAO/VBO, and stash each per-planet ring texture (the app's
// init(), under the "init: planetRing" milestone).
void Rings_InitGL(void);

// Additive alpha-weighted annulus overlay for each ringed planet, centred on
// the planet's world translation (from Planet_Transforms()); depth-tested, no
// depth write.
void Rings_Draw(const ViewMatrices *vm);

void Rings_Shutdown(void);

#endif
