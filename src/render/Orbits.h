#ifndef ORBITS_H
#define ORBITS_H

#include "Common.h"

// Compile the orbit-line program and upload the shared unit-circle VBO (the
// app's init(), called between the atmosphere and the "done" milestone).
void Orbits_InitGL(void);

// Faint white GL_LINE_LOOP of each planet's orbit radius: depth-tested so the
// planet bodies hide the far side, but the lines never write depth.
void Orbits_Draw(const ViewMatrices *vm);

#endif
